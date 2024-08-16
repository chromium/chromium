// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/service_worker/service_worker_installed_script_reader.h"

#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/blob/blob_utils.h"

namespace content {

class ServiceWorkerInstalledScriptReader::MetaDataSender {
 public:
  MetaDataSender(scoped_refptr<net::IOBufferWithSize> meta_data,
                 mojo::ScopedDataPipeProducerHandle handle)
      : meta_data_(std::move(meta_data)),
        bytes_sent_(0),
        handle_(std::move(handle)),
        watcher_(FROM_HERE,
                 mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
                 base::SequencedTaskRunner::GetCurrentDefault()) {}

  void Start(base::OnceCallback<void(bool /* success */)> callback) {
    callback_ = std::move(callback);
    watcher_.Watch(handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                   base::BindRepeating(&MetaDataSender::OnWritable,
                                       weak_factory_.GetWeakPtr()));
  }

  void OnWritable(MojoResult) {
    // It isn't necessary to handle MojoResult here since WriteDataRaw()
    // returns an equivalent error.
    base::span<const uint8_t> bytes_to_write =
        meta_data_->span().subspan(bytes_sent_);
    size_t actually_written_bytes = 0;
    TRACE_EVENT2(
        "ServiceWorker",
        "ServiceWorkerInstalledScriptReader::MetaDataSender::OnWritable",
        "meta_data size", meta_data_->size(), "bytes_sent_", bytes_sent_);
    MojoResult rv = handle_->WriteData(
        bytes_to_write, MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
    switch (rv) {
      case MOJO_RESULT_INVALID_ARGUMENT:
      case MOJO_RESULT_OUT_OF_RANGE:
      case MOJO_RESULT_BUSY:
        NOTREACHED_IN_MIGRATION();
        return;
      case MOJO_RESULT_FAILED_PRECONDITION:
        OnCompleted(false);
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        return;
      case MOJO_RESULT_OK:
        break;
      default:
        // mojo::WriteDataRaw() should not return any other values.
        OnCompleted(false);
        return;
    }
    bytes_sent_ += actually_written_bytes;
    TRACE_EVENT2(
        "ServiceWorker",
        "ServiceWorkerInstalledScriptReader::MetaDataSender::OnWritable",
        "meta_data size", meta_data_->size(), "new bytes_sent_", bytes_sent_);
    if (meta_data_->size() == bytes_sent_)
      OnCompleted(true);
  }

  void OnCompleted(bool success) {
    TRACE_EVENT0(
        "ServiceWorker",
        "ServiceWorkerInstalledScriptReader::MetaDataSender::OnComplete");
    watcher_.Cancel();
    handle_.reset();
    std::move(callback_).Run(success);
  }

 private:
  base::OnceCallback<void(bool /* success */)> callback_;

  scoped_refptr<net::IOBufferWithSize> meta_data_;
  int64_t bytes_sent_;
  mojo::ScopedDataPipeProducerHandle handle_;
  mojo::SimpleWatcher watcher_;

  base::WeakPtrFactory<MetaDataSender> weak_factory_{this};
};

ServiceWorkerInstalledScriptReader::ServiceWorkerInstalledScriptReader(
    mojo::Remote<storage::mojom::ServiceWorkerResourceReader> reader,
    Client* client)
    : reader_(std::move(reader)), client_(client) {
  DCHECK(reader_.is_connected());
  reader_.set_disconnect_handler(base::BindOnce(
      &ServiceWorkerInstalledScriptReader::OnReaderDisconnected, AsWeakPtr()));
}

ServiceWorkerInstalledScriptReader::~ServiceWorkerInstalledScriptReader() {}

void ServiceWorkerInstalledScriptReader::Start() {
  TRACE_EVENT0("ServiceWorker", "ServiceWorkerInstalledScriptReader::Start");
  DCHECK(reader_.is_connected());
  reader_->ReadResponseHead(base::BindOnce(
      &ServiceWorkerInstalledScriptReader::OnReadResponseHeadComplete,
      AsWeakPtr()));
}

void ServiceWorkerInstalledScriptReader::OnReadResponseHeadComplete(
    int result,
    network::mojom::URLResponseHeadPtr response_head,
    std::optional<mojo_base::BigBuffer> metadata) {
  DCHECK(client_);
  TRACE_EVENT0(
      "ServiceWorker",
      "ServiceWorkerInstalledScriptReader::OnReadResponseHeadComplete");
  if (!response_head) {
    DCHECK_LT(result, 0);
    ServiceWorkerMetrics::CountReadResponseResult(
        ServiceWorkerMetrics::READ_HEADERS_ERROR);
    CompleteSendIfNeeded(FinishedReason::kNoResponseHeadError);
    return;
  }

  DCHECK_GE(result, 0);
  DCHECK(reader_.is_connected());

  body_size_ = response_head->content_length;
  int64_t content_length = response_head->content_length;
  reader_->PrepareReadData(
      content_length,
      base::BindOnce(&ServiceWorkerInstalledScriptReader::OnReadDataPrepared,
                     AsWeakPtr(), std::move(response_head),
                     std::move(metadata)));
}

void ServiceWorkerInstalledScriptReader::OnReadDataPrepared(
    network::mojom::URLResponseHeadPtr response_head,
    std::optional<mojo_base::BigBuffer> metadata,
    mojo::ScopedDataPipeConsumerHandle body_consumer_handle) {
  if (!body_consumer_handle) {
    CompleteSendIfNeeded(FinishedReason::kCreateDataPipeError);
    return;
  }

  mojo::ScopedDataPipeConsumerHandle meta_data_consumer;

  // Start sending meta data (V8 code cache data).
  if (metadata) {
    DCHECK_GT(metadata->size(), 0UL);

    mojo::ScopedDataPipeProducerHandle meta_producer_handle;
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes =
        blink::BlobUtils::GetDataPipeCapacity(metadata->size());
    int rv = mojo::CreateDataPipe(&options, meta_producer_handle,
                                  meta_data_consumer);
    if (rv != MOJO_RESULT_OK) {
      CompleteSendIfNeeded(FinishedReason::kCreateDataPipeError);
      return;
    }

    // TODO(crbug.com/40120038): Avoid copying |metadata| if |client_| doesn't
    // need it.
    auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(metadata->size());
    memmove(buffer->data(), metadata->data(), metadata->size());
    meta_data_sender_ = std::make_unique<MetaDataSender>(
        std::move(buffer), std::move(meta_producer_handle));
    meta_data_sender_->Start(base::BindOnce(
        &ServiceWorkerInstalledScriptReader::OnMetaDataSent, AsWeakPtr()));
  }

  client_->OnStarted(std::move(response_head), std::move(metadata),
                     std::move(body_consumer_handle),
                     std::move(meta_data_consumer));

  reader_->ReadData(base::BindOnce(
      &ServiceWorkerInstalledScriptReader::OnComplete, AsWeakPtr()));
}

void ServiceWorkerInstalledScriptReader::OnReaderDisconnected() {
  CompleteSendIfNeeded(FinishedReason::kConnectionError);
}

void ServiceWorkerInstalledScriptReader::OnMetaDataSent(bool success) {
  meta_data_sender_.reset();
  if (!success) {
    CompleteSendIfNeeded(FinishedReason::kMetaDataSenderError);
    return;
  }

  CompleteSendIfNeeded(FinishedReason::kSuccess);
}

void ServiceWorkerInstalledScriptReader::CompleteSendIfNeeded(
    FinishedReason reason) {
  if (reason != FinishedReason::kSuccess) {
    client_->OnFinished(reason);
    return;
  }

  if (WasMetadataWritten() && WasBodyWritten())
    client_->OnFinished(reason);
}

void ServiceWorkerInstalledScriptReader::OnComplete(int32_t status) {
  was_body_written_ = true;
  if (status >= 0 && static_cast<uint64_t>(status) == body_size_) {
    ServiceWorkerMetrics::CountReadResponseResult(
        ServiceWorkerMetrics::READ_OK);
    CompleteSendIfNeeded(FinishedReason::kSuccess);
  } else if (status == net::ERR_ABORTED) {
    CompleteSendIfNeeded(FinishedReason::kConnectionError);
  } else {
    CompleteSendIfNeeded(FinishedReason::kResponseReaderError);
  }
}

}  // namespace content
