// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/command_queue.h"

#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"

namespace webnn::dml {

CommandQueue::CommandQueue(ComPtr<ID3D12CommandQueue> command_queue,
                           ComPtr<ID3D12Fence> fence)
    : base::win::ObjectWatcher::Delegate(),
      command_queue_(std::move(command_queue)),
      fence_(std::move(fence)) {
  fence_event_.Set(CreateEvent(nullptr, /*bManualReset=*/FALSE,
                               /*bInitialState=*/FALSE, nullptr));
  CHECK(fence_event_.is_valid());
}

CommandQueue::~CommandQueue() = default;

// static
scoped_refptr<CommandQueue> CommandQueue::Create(ID3D12Device* d3d12_device) {
  ComPtr<ID3D12CommandQueue> command_queue;
  D3D12_COMMAND_QUEUE_DESC command_queue_desc = {};
  command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  HRESULT hr = d3d12_device->CreateCommandQueue(&command_queue_desc,
                                                IID_PPV_ARGS(&command_queue));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create ID3D12CommandQueue: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  ComPtr<ID3D12Fence> fence;
  hr =
      d3d12_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create ID3D12Fence: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return base::WrapRefCounted(
      new CommandQueue(std::move(command_queue), std::move(fence)));
}

HRESULT CommandQueue::ExecuteCommandList(ID3D12CommandList* command_list) {
  return ExecuteCommandLists(base::make_span(&command_list, 1u));
}

HRESULT CommandQueue::ExecuteCommandLists(
    base::span<ID3D12CommandList*> command_lists) {
  command_queue_->ExecuteCommandLists(
      base::checked_cast<uint32_t>(command_lists.size()), command_lists.data());
  ++last_fence_value_;
  return command_queue_->Signal(fence_.Get(), last_fence_value_);
}

HRESULT CommandQueue::WaitSyncForTesting() {
  CHECK_IS_TEST();
  if (fence_->GetCompletedValue() >= last_fence_value_) {
    return S_OK;
  }

  HRESULT hr =
      fence_->SetEventOnCompletion(last_fence_value_, fence_event_.get());
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to set event on completion : "
                << logging::SystemErrorCodeToString(hr);
    return hr;
  };
  CHECK_EQ(WaitForSingleObject(fence_event_.get(), INFINITE), WAIT_OBJECT_0);
  return S_OK;
}

void CommandQueue::OnObjectSignaled(HANDLE object) {
  CHECK_EQ(object, fence_event_.get());
  while (!queued_callbacks_.empty() &&
         queued_callbacks_.front().fence_value <= fence_->GetCompletedValue()) {
    std::move(queued_callbacks_.front().callback).Run();
    queued_callbacks_.pop_front();
  }
}

HRESULT CommandQueue::WaitAsync(base::OnceClosure callback) {
  if (!object_watcher_.IsWatching()) {
    CHECK(object_watcher_.StartWatchingMultipleTimes(fence_event_.get(), this));
  }

  HRESULT hr =
      fence_->SetEventOnCompletion(last_fence_value_, fence_event_.get());
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to set event on completion : "
                << logging::SystemErrorCodeToString(hr);
    return hr;
  };
  queued_callbacks_.push_back({last_fence_value_, std::move(callback)});
  return S_OK;
}

void CommandQueue::ReferenceUntilCompleted(ComPtr<IUnknown> object) {
  queued_objects_.push_back({last_fence_value_, std::move(object)});
}

void CommandQueue::ReleaseCompletedResources() {
  uint64_t completed_value = fence_->GetCompletedValue();
  while (!queued_objects_.empty() &&
         queued_objects_.front().fence_value <= completed_value) {
    queued_objects_.pop_front();
  }
}

uint64_t CommandQueue::GetCompletedValue() const {
  return fence_->GetCompletedValue();
}

uint64_t CommandQueue::GetLastFenceValue() const {
  return last_fence_value_;
}

CommandQueue::QueuedObject::QueuedObject(uint64_t fence_value,
                                         ComPtr<IUnknown> object)
    : fence_value(fence_value), object(std::move(object)) {}
CommandQueue::QueuedObject::QueuedObject(QueuedObject&& other) = default;
CommandQueue::QueuedObject& CommandQueue::QueuedObject::operator=(
    QueuedObject&& other) = default;
CommandQueue::QueuedObject::~QueuedObject() = default;

CommandQueue::QueuedCallback::QueuedCallback(uint64_t fence_value,
                                             base::OnceClosure callback)
    : fence_value(fence_value), callback(std::move(callback)) {}
CommandQueue::QueuedCallback::QueuedCallback(QueuedCallback&& other) = default;
CommandQueue::QueuedCallback& CommandQueue::QueuedCallback::operator=(
    QueuedCallback&& other) = default;
CommandQueue::QueuedCallback::~QueuedCallback() = default;

}  // namespace webnn::dml
