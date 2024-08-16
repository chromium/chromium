// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/command_queue.h"

#include "base/check_is_test.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

CommandQueue::PendingWorkDelegate::PendingWorkDelegate(
    std::deque<CommandQueue::QueuedObject> queued_objects,
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue,
    uint64_t last_fence_value,
    Microsoft::WRL::ComPtr<ID3D12Fence> fence,
    base::win::ScopedHandle fence_event)
    : base::win::ObjectWatcher::Delegate(),
      queued_objects_(std::move(queued_objects)),
      command_queue_(std::move(command_queue)),
      last_fence_value_(last_fence_value),
      fence_(std::move(fence)),
      fence_event_(std::move(fence_event)) {
  CHECK(object_watcher_.StartWatchingOnce(fence_event_.get(), this));
}

CommandQueue::PendingWorkDelegate::~PendingWorkDelegate() = default;

//  Pending GPU work will be ensured done and signals the object watcher by
//  system eventually to delete PendingWorkDelegate, it's out of our control, we
//  have to trust that, if not, there will be memory leak.
void CommandQueue::PendingWorkDelegate::OnObjectSignaled(HANDLE object) {
  CHECK_EQ(object, fence_event_.get());

  CHECK_GE(fence_->GetCompletedValue(), last_fence_value_);
  delete this;
}

// static
void CommandQueue::ScheduleCleanupForPendingWork(
    std::deque<CommandQueue::QueuedObject> queued_objects,
    ComPtr<ID3D12CommandQueue> command_queue,
    uint64_t last_fence_value,
    ComPtr<ID3D12Fence> fence) {
  // Create a new fence_event that ensures it is only triggered by the
  // last_fence_value.
  base::win::ScopedHandle fence_event(
      CreateEvent(nullptr, /*bManualReset=*/FALSE,
                  /*bInitialState=*/FALSE, nullptr));
  CHECK(fence_event.is_valid());

  HRESULT hr = fence->SetEventOnCompletion(last_fence_value, fence_event.get());
  if (FAILED(hr)) {
    LOG(ERROR) << "[WebNN] Failed to set event on completion: "
               << logging::SystemErrorCodeToString(hr);
    return;
  }

  // It is by design we're not using std::unique_ptr because PendingWorkDelegate
  // deletes itself.
  new PendingWorkDelegate(std::move(queued_objects), std::move(command_queue),
                          last_fence_value, std::move(fence),
                          std::move(fence_event));
}

CommandQueue::CommandQueue(ComPtr<ID3D12CommandQueue> command_queue,
                           ComPtr<ID3D12Fence> fence)
    : base::win::ObjectWatcher::Delegate(),
      command_queue_(std::move(command_queue)),
      fence_(std::move(fence)) {
  fence_event_.Set(CreateEvent(nullptr, /*bManualReset=*/FALSE,
                               /*bInitialState=*/FALSE, nullptr));
  CHECK(fence_event_.is_valid());

  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CommandQueue::~CommandQueue() {
  if (fence_->GetCompletedValue() >= last_fence_value_) {
    return;
  }

  ScheduleCleanupForPendingWork(std::move(queued_objects_),
                                std::move(command_queue_), last_fence_value_,
                                std::move(fence_));
}

// static
scoped_refptr<CommandQueue> CommandQueue::Create(ID3D12Device* d3d12_device) {
  ComPtr<ID3D12CommandQueue> command_queue;
  D3D12_COMMAND_QUEUE_DESC command_queue_desc = {};
  command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
  // To avoid TDRs on account of long compute work, create WebNN command queues
  // with the "disable timeout" flag. If other compute work on the system
  // competes with WebNN, the scheduler will TDR us anyways if it cannot preempt
  // our queue.
  command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
  HRESULT hr = d3d12_device->CreateCommandQueue(&command_queue_desc,
                                                IID_PPV_ARGS(&command_queue));
  if (FAILED(hr)) {
    LOG(ERROR) << "[WebNN] Failed to create ID3D12CommandQueue: "
               << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  ComPtr<ID3D12Fence> fence;
  hr =
      d3d12_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
  if (FAILED(hr)) {
    LOG(ERROR) << "[WebNN] Failed to create ID3D12Fence: "
               << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return base::WrapRefCounted(
      new CommandQueue(std::move(command_queue), std::move(fence)));
}

HRESULT CommandQueue::ExecuteCommandList(ID3D12CommandList* command_list) {
  return ExecuteCommandLists(base::span_from_ref(command_list));
}

HRESULT CommandQueue::ExecuteCommandLists(
    base::span<ID3D12CommandList*> command_lists) {
  TRACE_EVENT0("gpu", "dml::CommandQueue::ExecuteCommandLists");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  command_queue_->ExecuteCommandLists(
      base::checked_cast<uint32_t>(command_lists.size()), command_lists.data());
  ++last_fence_value_;
  return command_queue_->Signal(fence_.Get(), last_fence_value_);
}

HRESULT CommandQueue::WaitSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (fence_->GetCompletedValue() >= last_fence_value_) {
    ReleaseCompletedResources();
    return S_OK;
  }

  HRESULT hr =
      fence_->SetEventOnCompletion(last_fence_value_, fence_event_.get());
  if (FAILED(hr)) {
    ReleaseCompletedResources();
    LOG(ERROR) << "Failed to set event on completion : "
               << logging::SystemErrorCodeToString(hr);
    return hr;
  }
  CHECK_EQ(WaitForSingleObject(fence_event_.get(), INFINITE), WAIT_OBJECT_0);
  ReleaseCompletedResources();
  return S_OK;
}

void CommandQueue::OnObjectSignaled(HANDLE object) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("gpu", "dml::CommandQueue::WaitAsync",
                                  TRACE_ID_LOCAL(this));
  CHECK_EQ(object, fence_event_.get());
  ReleaseCompletedResources();

  // If the owning document has shutdown by the time we get here, the only
  // remaining references to CommandQueue are inside of the queued_callbacks_
  // callbacks. Running them while continuing to dereference member variables
  // will free "this" and cause use-after-free  The caller of OnObjectSignaled,
  // ObjectWatcher::Signal, has a similar problem. To fix, we take a reference
  // to ourselves while we run the callbacks.
  scoped_refptr<CommandQueue> self = this;

  while (!queued_callbacks_.empty() &&
         queued_callbacks_.front().fence_value <= fence_->GetCompletedValue()) {
    std::move(queued_callbacks_.front().callback).Run();
    queued_callbacks_.pop_front();
  }
}

void CommandQueue::WaitAsync(base::OnceCallback<void(HRESULT hr)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!object_watcher_.IsWatching()) {
    CHECK(object_watcher_.StartWatchingMultipleTimes(fence_event_.get(), this));
  }

  HRESULT hr =
      fence_->SetEventOnCompletion(last_fence_value_, fence_event_.get());
  if (FAILED(hr)) {
    ReleaseCompletedResources();
    LOG(ERROR) << "[WebNN] Failed to set event on completion: "
               << logging::SystemErrorCodeToString(hr);
    std::move(callback).Run(hr);
    return;
  }
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("gpu", "dml::CommandQueue::WaitAsync",
                                    TRACE_ID_LOCAL(this));
  queued_callbacks_.push_back(
      {last_fence_value_, base::BindOnce(std::move(callback), S_OK)});
}

void CommandQueue::ReferenceUntilCompleted(ComPtr<IUnknown> object) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  queued_objects_.push_back({last_fence_value_, std::move(object)});
}

void CommandQueue::ReleaseCompletedResources() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_fence_value_;
}

const std::deque<CommandQueue::QueuedObject>&
CommandQueue::GetQueuedObjectsForTesting() const {
  CHECK_IS_TEST();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return queued_objects_;
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
