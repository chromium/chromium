// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_COMMAND_QUEUE_H_
#define SERVICES_WEBNN_DML_COMMAND_QUEUE_H_

#include <deque>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3d12.h"

// Windows SDK headers should be included after DirectX headers.
#include <wrl.h>

namespace webnn::dml {

// The CommandQueue is a wrapper of an ID3D12CommandQueue and contains a fence
// which is signaled when the execution on GPU is completed.
// Notice that the CommandQueue is not a thread-safe class, it should be used on
// the GPU main thread or the background thread via a sequenced task runner to
// avoid race conditions on its' member variables.
class COMPONENT_EXPORT(WEBNN_SERVICE) CommandQueue
    : public base::win::ObjectWatcher::Delegate,
      public base::RefCountedThreadSafe<CommandQueue> {
 public:
  static scoped_refptr<CommandQueue> Create(ID3D12Device* d3d12_device);

  CommandQueue(const CommandQueue&) = delete;
  CommandQueue& operator=(const CommandQueue&) = delete;

  HRESULT ExecuteCommandList(ID3D12CommandList* command_list);
  HRESULT ExecuteCommandLists(base::span<ID3D12CommandList*> command_lists);

  // It's a synchronous method for DirectML graph implementation, which will
  // block the CPU until the fence is signaled with the last fence value.
  // Calling it on the GPU main thread may block the UI. It must be called on
  // background thread in the production code.
  HRESULT WaitSync();

  // It's an asynchronous method for DirectML graph implementation, which will
  // not block the CPU. In case this method fails internally, `callback`
  // accepts a HRESULT from it to handle.
  void WaitAsync(base::OnceCallback<void(HRESULT hr)> callback);

  // The referenced resources will be released by command queue after the GPU
  // work using those resources has been completed.
  void ReferenceUntilCompleted(Microsoft::WRL::ComPtr<IUnknown> object);

  uint64_t GetCompletedValue() const;
  uint64_t GetLastFenceValue() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebNNCommandQueueTest, ReferenceAndRelease);

  friend class base::RefCountedThreadSafe<CommandQueue>;
  CommandQueue(Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue,
               Microsoft::WRL::ComPtr<ID3D12Fence> fence);
  ~CommandQueue() override;

  void ReleaseCompletedResources();

  struct QueuedObject {
    QueuedObject() = delete;
    QueuedObject(uint64_t fence_value, Microsoft::WRL::ComPtr<IUnknown> object);
    QueuedObject(QueuedObject&& other);
    QueuedObject& operator=(QueuedObject&& other);
    ~QueuedObject();

    uint64_t fence_value = 0;
    Microsoft::WRL::ComPtr<IUnknown> object;
  };
  std::deque<QueuedObject> queued_objects_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const std::deque<QueuedObject>& GetQueuedObjectsForTesting() const;

  struct QueuedCallback {
    QueuedCallback() = delete;
    QueuedCallback(uint64_t fence_value, base::OnceClosure callback);
    QueuedCallback(QueuedCallback&& other);
    QueuedCallback& operator=(QueuedCallback&& other);
    ~QueuedCallback();

    uint64_t fence_value = 0;
    base::OnceClosure callback;
  };
  std::deque<QueuedCallback> queued_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The PendingWorkDelegate is created in the destruction of CommandQueue if
  // there is still some pending work on GPU. CommandQueue transfers its queued
  // objects to the PendingWorkDelegate to ensure them alive because they may
  // still be used by the pending queued work on GPU. CommandQueue delegates to
  // PendingWorkDelegate to wait for all pending work on GPU to complete before
  // destructing CommandQueue itself. PendingWorkDelegate will delete itself
  // after all pending work is completed.
  class PendingWorkDelegate : public base::win::ObjectWatcher::Delegate {
   public:
    PendingWorkDelegate(
        std::deque<CommandQueue::QueuedObject> queued_objects,
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue,
        uint64_t last_fence_value,
        Microsoft::WRL::ComPtr<ID3D12Fence> fence,
        base::win::ScopedHandle fence_event);
    ~PendingWorkDelegate() override;

    PendingWorkDelegate(const PendingWorkDelegate&) = delete;
    PendingWorkDelegate& operator=(const PendingWorkDelegate&) = delete;

   private:
    // Implements base::win::ObjectWatcher::Delegate.
    void OnObjectSignaled(HANDLE object) override;

    std::deque<CommandQueue::QueuedObject> queued_objects_;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue_;

    // The fence value is used to track the progress of GPU execution
    // work. Comparing it with the fence's completed value can indicate whether
    // the work has been completed.
    const uint64_t last_fence_value_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

    base::win::ScopedHandle fence_event_;
    base::win::ObjectWatcher object_watcher_;
  };

  // Implements base::win::ObjectWatcher::Delegate.
  void OnObjectSignaled(HANDLE object) override;

  static void ScheduleCleanupForPendingWork(
      std::deque<CommandQueue::QueuedObject> queued_objects,
      Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue,
      uint64_t last_fence_value,
      Microsoft::WRL::ComPtr<ID3D12Fence> fence);

  Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The increasing fence value is used to track the progress of GPU execution
  // work. Comparing it with the fence's completed value can indicate whether
  // the work has been completed.
  uint64_t last_fence_value_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  // `ID3D12Fence::SetEventOnCompletion` is only called by
  // `CommandQueue::WaitSync` and `CommandQueue::WaitAsync`, both methods
  // are guaranteed to be called on one sequence (by
  // DCHECK_CALLED_ON_VALID_SEQUENCE). Additionally,
  // `ID3D12Fence::GetCompletedValue` is called by
  // `CommandQueue::GetCompletedValue` which is used by `CommandRecorder::Open`
  // on gpuMain thread. Because `ID3D12Fence::GetCompletedValue` is thread-safe,
  // it doesn't need to be protected by GUARDED_BY_CONTEXT.
  Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

  base::win::ScopedHandle fence_event_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::win::ObjectWatcher object_watcher_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_COMMAND_QUEUE_H_
