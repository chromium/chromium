// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_COMMAND_QUEUE_H_
#define SERVICES_WEBNN_DML_COMMAND_QUEUE_H_

#include <d3d12.h>
#include <wrl.h>
#include <deque>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"

namespace webnn::dml {

// The CommandQueue is a wrapper of an ID3D12CommandQueue and contains a fence
// which is signaled when the execution on GPU is completed.
class CommandQueue : public base::win::ObjectWatcher::Delegate,
                     public base::RefCounted<CommandQueue> {
 public:
  static scoped_refptr<CommandQueue> Create(ID3D12Device* d3d12_device);

  CommandQueue(const CommandQueue&) = delete;
  CommandQueue& operator=(const CommandQueue&) = delete;

  HRESULT ExecuteCommandList(ID3D12CommandList* command_list);
  HRESULT ExecuteCommandLists(base::span<ID3D12CommandList*> command_lists);

  // It's a synchronous method only for testing, which will block the CPU until
  // the fence is signaled with the last fence value. Calling it on the GPU main
  // thread may block the UI.
  HRESULT WaitSyncForTesting();

  using OnWaitAyncCallback = base::OnceCallback<void(HRESULT hr)>;
  // It's an asynchronous method for DirectML graph implementation, which will
  // not block the CPU. In case this method fails internally, the
  // OnWaitAyncCallback accepts a HRESULT from it to handle.
  void WaitAsync(OnWaitAyncCallback callback);

  void ReferenceUntilCompleted(Microsoft::WRL::ComPtr<IUnknown> object);
  void ReleaseCompletedResources();

  uint64_t GetCompletedValue() const;
  uint64_t GetLastFenceValue() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebNNCommandQueueTest, ReferenceAndRelease);

  friend class base::RefCounted<CommandQueue>;
  CommandQueue(Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue,
               Microsoft::WRL::ComPtr<ID3D12Fence> fence);
  ~CommandQueue() override;

  struct QueuedObject {
    QueuedObject() = delete;
    QueuedObject(uint64_t fence_value, Microsoft::WRL::ComPtr<IUnknown> object);
    QueuedObject(QueuedObject&& other);
    QueuedObject& operator=(QueuedObject&& other);
    ~QueuedObject();

    uint64_t fence_value = 0;
    Microsoft::WRL::ComPtr<IUnknown> object;
  };
  std::deque<QueuedObject> queued_objects_;

  struct QueuedCallback {
    QueuedCallback() = delete;
    QueuedCallback(uint64_t fence_value, base::OnceClosure callback);
    QueuedCallback(QueuedCallback&& other);
    QueuedCallback& operator=(QueuedCallback&& other);
    ~QueuedCallback();

    uint64_t fence_value = 0;
    base::OnceClosure callback;
  };
  std::deque<QueuedCallback> queued_callbacks_;

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
        Microsoft::WRL::ComPtr<ID3D12Fence> fence);
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

  Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue_;

  // The increasing fence value is used to track the progress of GPU execution
  // work. Comparing it with the fence's completed value can indicate whether
  // the work has been completed.
  uint64_t last_fence_value_ = 0;
  Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

  base::win::ScopedHandle fence_event_;
  base::win::ObjectWatcher object_watcher_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_COMMAND_QUEUE_H_
