// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/race_network_request_write_buffer_manager.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "content/common/features.h"
#include "content/common/service_worker/race_network_request_url_loader_client.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/cpp/features.h"

namespace content {
namespace {
MojoResult CreateDataPipe(mojo::ScopedDataPipeProducerHandle& producer_handle,
                          mojo::ScopedDataPipeConsumerHandle& consumer_handle,
                          uint32_t capacity_num_bytes) {
  MojoCreateDataPipeOptions options;

  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = capacity_num_bytes;

  return mojo::CreateDataPipe(&options, producer_handle, consumer_handle);
}
}  // namespace

uint32_t RaceNetworkRequestWriteBufferManager::data_pipe_size_for_test_ = 0;

RaceNetworkRequestWriteBufferManager::RaceNetworkRequestWriteBufferManager()
    : data_pipe_buffer_size_(GetDataPipeCapacityBytes()),
      watcher_(FROM_HERE,
               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
               base::SequencedTaskRunner::GetCurrentDefault()) {
  MojoResult result =
      CreateDataPipe(producer_, consumer_, data_pipe_buffer_size_);
  is_data_pipe_created_ = result == MOJO_RESULT_OK;
}

RaceNetworkRequestWriteBufferManager::~RaceNetworkRequestWriteBufferManager() =
    default;

uint32_t RaceNetworkRequestWriteBufferManager::GetDataPipeCapacityBytes() {
  if (data_pipe_size_for_test_ > 0) {
    return data_pipe_size_for_test_;
  }
  // The feature param may override the buffer size.
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kServiceWorkerAutoPreload, "data_pipe_capacity_num_bytes",
      network::features::GetDataPipeDefaultAllocationSize(
          network::features::DataPipeAllocationSize::kLargerSizeIfPossible));
}

mojo::ScopedDataPipeConsumerHandle
RaceNetworkRequestWriteBufferManager::ReleaseConsumerHandle() {
  return std::move(consumer_);
}

void RaceNetworkRequestWriteBufferManager::Abort() {
  producer_.reset();
  consumer_.reset();
  watcher_.Cancel();
}

void RaceNetworkRequestWriteBufferManager::ResetProducer() {
  producer_.reset();
}

void RaceNetworkRequestWriteBufferManager::Watch(
    mojo::SimpleWatcher::ReadyCallbackWithState callback) {
  watcher_.Watch(producer_.get(),
                 MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                 MOJO_WATCH_CONDITION_SATISFIED, std::move(callback));
}

void RaceNetworkRequestWriteBufferManager::CancelWatching() {
  watcher_.Cancel();
}

MojoResult RaceNetworkRequestWriteBufferManager::BeginWriteData() {
  void* buffer;
  size_t num_write_bytes;
  MojoResult result = producer_->BeginWriteData(&buffer, &num_write_bytes,
                                                MOJO_WRITE_DATA_FLAG_NONE);
  buffer_ = base::make_span(static_cast<char*>(buffer), num_write_bytes);

  return result;
}

MojoResult RaceNetworkRequestWriteBufferManager::EndWriteData(
    uint32_t num_bytes_written) {
  return producer_->EndWriteData(num_bytes_written);
}

void RaceNetworkRequestWriteBufferManager::ArmOrNotify() {
  watcher_.ArmOrNotify();
}

std::tuple<MojoResult, size_t> RaceNetworkRequestWriteBufferManager::WriteData(
    base::span<const char> read_buffer) {
  // In order to use |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| flag to write data, the
  // read buffer data size should be smaller than the write buffer size.
  // Otherwise we can't finish the write operation nad `WriteData()` always
  // return |MOJO_RESULT_OUT_OF_RANGE|.
  auto buffer = read_buffer.size() > data_pipe_buffer_size_
                    ? read_buffer.subspan(0, data_pipe_buffer_size_)
                    : read_buffer;
  size_t num_bytes = buffer.size();
  MojoResult result = producer_->WriteData(buffer.data(), &num_bytes,
                                           MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  num_bytes_written_ += num_bytes;

  return {result, num_bytes};
}

size_t RaceNetworkRequestWriteBufferManager::CopyAndCompleteWriteData(
    base::span<const char> read_buffer) {
  return CopyAndCompleteWriteDataWithSize(read_buffer, read_buffer.size());
}

size_t RaceNetworkRequestWriteBufferManager::CopyAndCompleteWriteDataWithSize(
    base::span<const char> read_buffer,
    size_t max_num_bytes_to_consume) {
  // Choose smaller one from either read buffer, write buffer, or
  // |max_num_bytes_to_consume|.
  size_t num_bytes_to_consume =
      std::min({buffer_size(), read_buffer.size(), max_num_bytes_to_consume});

  SCOPED_CRASH_KEY_NUMBER("SWRace", "physical_memory_mb",
                          base::SysInfo::AmountOfPhysicalMemoryMB());
  SCOPED_CRASH_KEY_NUMBER("SWRace", "num_bytes_to_consume",
                          num_bytes_to_consume);

  CHECK_GE(data_pipe_buffer_size_, num_bytes_to_consume);
  CHECK_GE(buffer_size(), num_bytes_to_consume);
  CHECK_GE(read_buffer.size(), num_bytes_to_consume);
  // Check if all memory spaces are available to access. `volatile` to avoid the
  // compiler optimization.
  // TODO(crbug.com/1502946) Remove this code once we confirmed the root cause
  // of the crash.
  volatile const char* read_buffer_v =
      static_cast<volatile const char*>(read_buffer.data());
  for (size_t i = 0; i < read_buffer.size(); ++i) {
    read_buffer_v[i];
  }
  memcpy(buffer_.data(), read_buffer.data(), num_bytes_to_consume);
  MojoResult result = EndWriteData(num_bytes_to_consume);
  CHECK_EQ(result, MOJO_RESULT_OK);

  return num_bytes_to_consume;
}

// static
void RaceNetworkRequestWriteBufferManager::SetDataPipeCapacityBytesForTesting(
    uint32_t size) {
  data_pipe_size_for_test_ = size;
}
}  // namespace content
