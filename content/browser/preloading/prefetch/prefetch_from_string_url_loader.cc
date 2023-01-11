// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_from_string_url_loader.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetched_mainframe_response_container.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/io_buffer.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

PrefetchFromStringURLLoader::PrefetchFromStringURLLoader(
    std::unique_ptr<PrefetchedMainframeResponseContainer> response,
    const absl::optional<PrefetchResponseSizes>& response_sizes,
    const network::ResourceRequest& tentative_resource_request)
    : head_(response->ReleaseHead()),
      body_buffer_(
          base::MakeRefCounted<net::StringIOBuffer>(response->ReleaseBody())),
      bytes_of_raw_data_to_transfer_(body_buffer_->size()),
      response_sizes_(response_sizes) {}

PrefetchFromStringURLLoader::~PrefetchFromStringURLLoader() = default;

void PrefetchFromStringURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  NOTREACHED();
}

void PrefetchFromStringURLLoader::SetPriority(net::RequestPriority priority,
                                              int32_t intra_priority_value) {
  // Ignore: this class doesn't have a concept of priority.
}

void PrefetchFromStringURLLoader::PauseReadingBodyFromNet() {
  // Ignore: this class doesn't read from network.
}

void PrefetchFromStringURLLoader::ResumeReadingBodyFromNet() {
  // Ignore: this class doesn't read from network.
}

void PrefetchFromStringURLLoader::TransferRawData() {
  while (true) {
    DCHECK_GE(bytes_of_raw_data_to_transfer_, write_position_);
    uint32_t write_size =
        static_cast<uint32_t>(bytes_of_raw_data_to_transfer_ - write_position_);
    if (write_size == 0) {
      Finish(net::OK);
      return;
    }

    MojoResult result =
        producer_handle_->WriteData(body_buffer_->data() + write_position_,
                                    &write_size, MOJO_WRITE_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      handle_watcher_->ArmOrNotify();
      return;
    }

    if (result != MOJO_RESULT_OK) {
      Finish(net::ERR_FAILED);
      return;
    }

    // |write_position_| should only be updated when the mojo pipe has
    // successfully been written to.
    write_position_ += write_size;
  }
}

PrefetchFromStringURLLoader::RequestHandler
PrefetchFromStringURLLoader::ServingResponseHandler() {
  return base::BindOnce(&PrefetchFromStringURLLoader::BindAndStart,
                        weak_ptr_factory_.GetWeakPtr());
}

void PrefetchFromStringURLLoader::BindAndStart(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&PrefetchFromStringURLLoader::OnMojoDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));
  client_.Bind(std::move(client));

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult rv =
      mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle);

  if (rv != MOJO_RESULT_OK) {
    Finish(net::ERR_FAILED);
    return;
  }

  client_->OnReceiveResponse(std::move(head_), std::move(consumer_handle),
                             absl::nullopt);

  producer_handle_ = std::move(producer_handle);

  handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      base::SequencedTaskRunner::GetCurrentDefault());
  handle_watcher_->Watch(
      producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&PrefetchFromStringURLLoader::OnHandleReady,
                          weak_ptr_factory_.GetWeakPtr()));

  TransferRawData();
}

void PrefetchFromStringURLLoader::OnHandleReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    Finish(net::ERR_FAILED);
    return;
  }
  TransferRawData();
}

void PrefetchFromStringURLLoader::Finish(int error) {
  network::URLLoaderCompletionStatus status(error);
  if (response_sizes_) {
    status.encoded_data_length = response_sizes_->encoded_data_length;
    status.encoded_body_length = response_sizes_->encoded_body_length;
    status.decoded_body_length = response_sizes_->decoded_body_length;
  }
  client_->OnComplete(status);
  handle_watcher_.reset();
  producer_handle_.reset();
  client_.reset();
  receiver_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  MaybeDeleteSelf();
}

void PrefetchFromStringURLLoader::OnMojoDisconnect() {
  receiver_.reset();
  client_.reset();
  MaybeDeleteSelf();
}

void PrefetchFromStringURLLoader::MaybeDeleteSelf() {
  if (!receiver_.is_bound() && !client_.is_bound()) {
    delete this;
  }
}

}  // namespace content
