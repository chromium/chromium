// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/command_queue.h"

#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace webnn::dml {

CommandQueue::CommandQueue(ComPtr<ID3D12CommandQueue> command_queue,
                           ComPtr<ID3D12Fence> fence)
    : command_queue_(std::move(command_queue)), fence_(std::move(fence)) {
  fence_event_.Set(CreateEvent(nullptr, FALSE, FALSE, nullptr));
  CHECK(fence_event_.is_valid());
}

CommandQueue::~CommandQueue() = default;

// static
std::unique_ptr<CommandQueue> CommandQueue::Create(ID3D12Device* d3d12_device) {
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

  return base::WrapUnique(
      new CommandQueue(std::move(command_queue), std::move(fence)));
}

void CommandQueue::ReferenceUntilCompleted(ComPtr<IUnknown> object) {
  QueuedObject queue_object = {last_fence_value_, std::move(object)};
  queued_objects_.push_back(queue_object);
}

HRESULT CommandQueue::ExecuteCommandLists(
    const std::vector<ID3D12CommandList*>& command_lists) {
  command_queue_->ExecuteCommandLists(command_lists.size(),
                                      command_lists.data());
  ++last_fence_value_;
  return command_queue_->Signal(fence_.Get(), last_fence_value_);
}

void CommandQueue::WaitForTesting() {
  CHECK_IS_TEST();
  if (fence_->GetCompletedValue() >= last_fence_value_) {
    return;
  }
  HRESULT hr =
      fence_->SetEventOnCompletion(last_fence_value_, fence_event_.Get());
  if (FAILED(hr)) {
    return;
  }
  CHECK_EQ(WaitForSingleObject(fence_event_.Get(), INFINITE), WAIT_OBJECT_0);
}

void CommandQueue::ReleaseCompletedResources() {
  uint64_t completed_value = fence_->GetCompletedValue();
  while (!queued_objects_.empty() &&
         queued_objects_.front().fence_value <= completed_value) {
    queued_objects_.pop_front();
  }
}

CommandQueue::QueuedObject::QueuedObject(uint64_t fence_value,
                                         ComPtr<IUnknown> object) {
  this->fence_value = fence_value;
  this->object = std::move(object);
}

CommandQueue::QueuedObject::QueuedObject(const QueuedObject& other) = default;

CommandQueue::QueuedObject::QueuedObject() = default;
CommandQueue::QueuedObject::~QueuedObject() = default;

}  // namespace webnn::dml
