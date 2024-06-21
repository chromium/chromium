// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_data_pipe_writer.h"

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/shared_dictionary/shared_dictionary_writer.h"

namespace network {

// static

uint32_t SharedDictionaryDataPipeWriter::GetDataPipeBufferSize() {
  return network::features::GetDataPipeDefaultAllocationSize(
      features::DataPipeAllocationSize::kLargerSizeIfPossible);
}

// static
std::unique_ptr<SharedDictionaryDataPipeWriter>
SharedDictionaryDataPipeWriter::Create(
    mojo::ScopedDataPipeConsumerHandle& body,
    scoped_refptr<SharedDictionaryWriter> writer,
    base::OnceCallback<void(bool)> finish_callback) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = GetDataPipeBufferSize();

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult result =
      mojo::CreateDataPipe(&options, producer_handle, consumer_handle);
  if (result != MOJO_RESULT_OK) {
    return nullptr;
  }
  auto data_pipe_writer = base::WrapUnique(new SharedDictionaryDataPipeWriter(
      std::move(body), std::move(producer_handle), std::move(writer),
      std::move(finish_callback)));
  body = std::move(consumer_handle);
  return data_pipe_writer;
}

SharedDictionaryDataPipeWriter::SharedDictionaryDataPipeWriter(
    mojo::ScopedDataPipeConsumerHandle consumer_handle,
    mojo::ScopedDataPipeProducerHandle producer_handle,
    scoped_refptr<SharedDictionaryWriter> writer,
    base::OnceCallback<void(bool)> finish_callback)
    : consumer_handle_(std::move(consumer_handle)),
      producer_handle_(std::move(producer_handle)),
      writer_(std::move(writer)),
      consumer_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      producer_writable_watcher_(FROM_HERE,
                                 mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      producer_closed_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      finish_callback_(std::move(finish_callback)) {
  consumer_watcher_.Watch(
      consumer_handle_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&SharedDictionaryDataPipeWriter::ContinueReadWrite,
                          base::Unretained(this)));
  producer_writable_watcher_.Watch(
      producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&SharedDictionaryDataPipeWriter::ContinueReadWrite,
                          base::Unretained(this)));
  producer_closed_watcher_.Watch(
      producer_handle_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&SharedDictionaryDataPipeWriter::OnPeerClosed,
                          base::Unretained(this)));

  consumer_watcher_.ArmOrNotify();
  producer_closed_watcher_.ArmOrNotify();
}

SharedDictionaryDataPipeWriter::~SharedDictionaryDataPipeWriter() = default;

void SharedDictionaryDataPipeWriter::OnComplete(bool success) {
  DCHECK(!completion_result_.has_value());
  completion_result_ = success;
  MaybeFinish();
}

void SharedDictionaryDataPipeWriter::ContinueReadWrite(
    MojoResult,
    const mojo::HandleSignalsState& state) {
  base::span<const uint8_t> buffer;
  MojoResult result =
      consumer_handle_->BeginReadData(MOJO_BEGIN_READ_DATA_FLAG_NONE, buffer);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // Successfully read the whole data.
      FinishDataPipeOperation(/*success=*/true);
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      // `consumer_handle_` must be readable or closed here.
      NOTREACHED_IN_MIGRATION();
      return;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }

  size_t actually_written_bytes = 0;
  result = producer_handle_->WriteData(buffer, MOJO_WRITE_DATA_FLAG_NONE,
                                       actually_written_bytes);
  buffer = buffer.first(actually_written_bytes);
  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_SHOULD_WAIT:
      consumer_handle_->EndReadData(0);
      producer_writable_watcher_.ArmOrNotify();
      return;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // The data pipe consumer is aborted.
      FinishDataPipeOperation(/*success=*/false);
      return;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  std::string_view chars = base::as_string_view(buffer);
  writer_->Append(chars.data(), chars.size());
  consumer_handle_->EndReadData(buffer.size());
  consumer_watcher_.ArmOrNotify();
}

void SharedDictionaryDataPipeWriter::OnPeerClosed(
    MojoResult,
    const mojo::HandleSignalsState& state) {
  // The data pipe consumer is aborted.
  FinishDataPipeOperation(/*success=*/false);
}

void SharedDictionaryDataPipeWriter::FinishDataPipeOperation(bool success) {
  DCHECK(!data_pipe_operation_result_.has_value());
  data_pipe_operation_result_ = success;

  consumer_watcher_.Cancel();
  producer_writable_watcher_.Cancel();
  producer_closed_watcher_.Cancel();
  consumer_handle_.reset();
  producer_handle_.reset();

  MaybeFinish();
}

void SharedDictionaryDataPipeWriter::MaybeFinish() {
  if (!writer_ || !finish_callback_) {
    return;
  }

  if (!data_pipe_operation_result_.value_or(true) ||
      !completion_result_.value_or(true)) {
    // The data pipe consumer is aborted, or OnComplete() is called with false.
    writer_.reset();
    std::move(finish_callback_).Run(false);
    return;
  }

  if (!data_pipe_operation_result_.has_value() ||
      !completion_result_.has_value()) {
    return;
  }

  DCHECK(*data_pipe_operation_result_);
  DCHECK(*completion_result_);
  // Successfully read the whole data, and OnComplete() is called with true.
  writer_->Finish();
  writer_.reset();
  std::move(finish_callback_).Run(true);
}

}  // namespace network
