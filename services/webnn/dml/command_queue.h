// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_COMMAND_QUEUE_H_
#define SERVICES_WEBNN_DML_COMMAND_QUEUE_H_

#include <DirectML.h>
#include <d3d12.h>
#include <wrl.h>

#include <deque>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/win/scoped_handle.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

// The CommandQueue is a wrapper of an ID3D12CommandQueue and contains a fence
// which is signaled when the execution on GPU is completed.
class CommandQueue {
 public:
  static std::unique_ptr<CommandQueue> Create(ID3D12Device* d3d12_device);

  CommandQueue(const CommandQueue&) = delete;
  CommandQueue& operator=(const CommandQueue&) = delete;
  ~CommandQueue();

  void ReferenceUntilCompleted(ComPtr<IUnknown> object);
  HRESULT ExecuteCommandLists(
      const std::vector<ID3D12CommandList*>& command_lists);

  // It's a synchronous method only for testing, which will block the GPU until
  // the fence is signaled with the last fence value. Calling it on the GPU main
  // thread may block the UI.
  //
  // TODO(crbug.com/1273291): Add asynchronous WaitAsync() by using
  // base::WaitableEventWatcher.
  void WaitForTesting();
  void ReleaseCompletedResources();

 private:
  FRIEND_TEST_ALL_PREFIXES(WebNNCommandQueueTest, ReferenceAndRelease);

  CommandQueue(ComPtr<ID3D12CommandQueue> command_queue,
               ComPtr<ID3D12Fence> fence);

  struct QueuedObject {
    QueuedObject();
    QueuedObject(const QueuedObject& other);
    QueuedObject(uint64_t fence_value, ComPtr<IUnknown> object);
    ~QueuedObject();

    uint64_t fence_value = 0;
    ComPtr<IUnknown> object;
  };
  std::deque<QueuedObject> queued_objects_;

  ComPtr<ID3D12CommandQueue> command_queue_;

  // The increasing fence value is used to track the progress of GPU execution
  // work. Comparing it with the fence's completed value can indicate whether
  // the work has been completed.
  uint64_t last_fence_value_ = 0;
  ComPtr<ID3D12Fence> fence_;
  base::win::ScopedHandle fence_event_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_COMMAND_QUEUE_H_
