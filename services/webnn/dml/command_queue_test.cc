// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wrl.h>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

class WebNNCommandQueueTest : public TestBase {
 public:
  void SetUp() override;

 protected:
  ComPtr<ID3D12Device> d3d12_device_;
};

void WebNNCommandQueueTest::SetUp() {
  SKIP_TEST_IF(!UseGPUInTests());
  Adapter::EnableDebugLayerForTesting();
  auto adapter_creation_result = Adapter::GetGpuInstanceForTesting();
  // If the adapter creation result has no value, it's most likely because
  // platform functions were not properly loaded.
  SKIP_TEST_IF(!adapter_creation_result.has_value());
  d3d12_device_ = adapter_creation_result.value()->d3d12_device();
}

TEST_F(WebNNCommandQueueTest, CreateCommandQueue) {
  EXPECT_NE(CommandQueue::Create(d3d12_device_.Get()), nullptr);
}

TEST_F(WebNNCommandQueueTest, WaitSyncForGpuWorkCompleted) {
  ASSERT_NE(d3d12_device_.Get(), nullptr);
  ComPtr<ID3D12CommandAllocator> command_allocator;
  ASSERT_EQ(
      (d3d12_device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                             IID_PPV_ARGS(&command_allocator))),
      S_OK);
  ComPtr<ID3D12GraphicsCommandList> command_list;
  ASSERT_EQ(d3d12_device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                             command_allocator.Get(), nullptr,
                                             IID_PPV_ARGS(&command_list)),
            S_OK);
  scoped_refptr<CommandQueue> command_queue =
      CommandQueue::Create(d3d12_device_.Get());
  ASSERT_NE(command_queue.get(), nullptr);
  ASSERT_EQ(command_list->Close(), S_OK);
  EXPECT_EQ(command_queue->ExecuteCommandList(command_list.Get()), S_OK);
  EXPECT_EQ(command_queue->WaitSync(), S_OK);
  EXPECT_EQ(command_allocator->Reset(), S_OK);
  EXPECT_EQ(command_list->Reset(command_allocator.Get(), nullptr), S_OK);
}

TEST_F(WebNNCommandQueueTest, WaitAsyncOnce) {
  ASSERT_NE(d3d12_device_.Get(), nullptr);
  ComPtr<ID3D12CommandAllocator> command_allocator;
  ASSERT_EQ(
      (d3d12_device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                             IID_PPV_ARGS(&command_allocator))),
      S_OK);
  ComPtr<ID3D12GraphicsCommandList> command_list;
  ASSERT_EQ(d3d12_device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                             command_allocator.Get(), nullptr,
                                             IID_PPV_ARGS(&command_list)),
            S_OK);
  scoped_refptr<CommandQueue> command_queue =
      CommandQueue::Create(d3d12_device_.Get());
  ASSERT_NE(command_queue.get(), nullptr);
  ASSERT_EQ(command_list->Close(), S_OK);
  EXPECT_EQ(command_queue->ExecuteCommandList(command_list.Get()), S_OK);

  base::test::TestFuture<HRESULT> future;
  command_queue->WaitAsync(future.GetCallback());
  EXPECT_EQ(future.Take(), S_OK);

  EXPECT_EQ(command_allocator->Reset(), S_OK);
  EXPECT_EQ(command_list->Reset(command_allocator.Get(), nullptr), S_OK);
}

TEST_F(WebNNCommandQueueTest, WaitAsyncMultipleTimesOnIncreasingFenceValue) {
  ASSERT_NE(d3d12_device_.Get(), nullptr);
  ComPtr<ID3D12CommandAllocator> command_allocator;
  ASSERT_EQ(
      (d3d12_device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                             IID_PPV_ARGS(&command_allocator))),
      S_OK);
  ComPtr<ID3D12GraphicsCommandList> command_list;
  ASSERT_EQ(d3d12_device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                             command_allocator.Get(), nullptr,
                                             IID_PPV_ARGS(&command_list)),
            S_OK);
  scoped_refptr<CommandQueue> command_queue =
      CommandQueue::Create(d3d12_device_.Get());
  ASSERT_NE(command_queue.get(), nullptr);
  ASSERT_EQ(command_list->Close(), S_OK);
  EXPECT_EQ(command_queue->ExecuteCommandList(command_list.Get()), S_OK);

  int32_t count = 2;
  base::RunLoop run_loop;

  // Call WaitAsync for the first time with fence value 1.
  command_queue->WaitAsync(base::BindLambdaForTesting([&](HRESULT hr) {
    EXPECT_EQ(hr, S_OK);
    if (--count) {
      return;
    } else {
      run_loop.Quit();
    }
  }));

  EXPECT_EQ(command_allocator->Reset(), S_OK);
  EXPECT_EQ(command_list->Reset(command_allocator.Get(), nullptr), S_OK);

  // Call WaitAsync for the second time with fence value 2.
  ASSERT_EQ(command_list->Close(), S_OK);
  EXPECT_EQ(command_queue->ExecuteCommandList(command_list.Get()), S_OK);
  command_queue->WaitAsync(base::BindLambdaForTesting([&](HRESULT hr) {
    EXPECT_EQ(hr, S_OK);
    if (--count) {
      return;
    } else {
      run_loop.Quit();
    }
  }));

  run_loop.Run();
  EXPECT_EQ(count, 0);
  EXPECT_EQ(command_allocator->Reset(), S_OK);
  EXPECT_EQ(command_list->Reset(command_allocator.Get(), nullptr), S_OK);
}

TEST_F(WebNNCommandQueueTest, WaitAsyncMultipleTimesOnSameFenceValue) {
  ASSERT_NE(d3d12_device_.Get(), nullptr);
  ComPtr<ID3D12CommandAllocator> command_allocator;
  ASSERT_EQ(
      (d3d12_device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                             IID_PPV_ARGS(&command_allocator))),
      S_OK);
  ComPtr<ID3D12GraphicsCommandList> command_list;
  ASSERT_EQ(d3d12_device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                             command_allocator.Get(), nullptr,
                                             IID_PPV_ARGS(&command_list)),
            S_OK);
  scoped_refptr<CommandQueue> command_queue =
      CommandQueue::Create(d3d12_device_.Get());
  ASSERT_NE(command_queue.get(), nullptr);
  ASSERT_EQ(command_list->Close(), S_OK);
  EXPECT_EQ(command_queue->ExecuteCommandList(command_list.Get()), S_OK);

  int32_t count = 2;
  base::RunLoop run_loop;

  // Call WaitAsync for the first time with fence value 1.
  command_queue->WaitAsync(base::BindLambdaForTesting([&](HRESULT hr) {
    EXPECT_EQ(hr, S_OK);
    if (--count) {
      return;
    } else {
      run_loop.Quit();
    }
  }));

  // Call WaitAsync for the second time on the same fence value 1.
  command_queue->WaitAsync(base::BindLambdaForTesting([&](HRESULT hr) {
    EXPECT_EQ(hr, S_OK);
    if (--count) {
      return;
    } else {
      run_loop.Quit();
    }
  }));

  run_loop.Run();
  EXPECT_EQ(count, 0);
  EXPECT_EQ(command_allocator->Reset(), S_OK);
  EXPECT_EQ(command_list->Reset(command_allocator.Get(), nullptr), S_OK);
}

TEST_F(WebNNCommandQueueTest, ReferenceAndRelease) {
  scoped_refptr<CommandQueue> command_queue =
      CommandQueue::Create(d3d12_device_.Get());
  ASSERT_NE(command_queue.get(), nullptr);

  D3D12_HEAP_PROPERTIES heap_properties;
  heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
  heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heap_properties.CreationNodeMask = 1;
  heap_properties.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC resource_desc;
  resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resource_desc.Alignment = 0;
  resource_desc.Width = 16;
  resource_desc.Height = 1;
  resource_desc.DepthOrArraySize = 1;
  resource_desc.MipLevels = 1;
  resource_desc.Format = DXGI_FORMAT_UNKNOWN;
  resource_desc.SampleDesc = {1, 0};
  resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

  ComPtr<ID3D12Resource> resource;
  ASSERT_EQ(d3d12_device_->CreateCommittedResource(
                &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                IID_PPV_ARGS(&resource)),
            S_OK);
  ASSERT_NE(resource.Get(), nullptr);
  const std::deque<CommandQueue::QueuedObject>& queued_objects =
      command_queue->GetQueuedObjectsForTesting();
  EXPECT_EQ(queued_objects.size(), 0u);
  command_queue->ReferenceUntilCompleted(std::move(resource));
  EXPECT_EQ(queued_objects.size(), 1u);
  command_queue->ReleaseCompletedResources();
  EXPECT_EQ(queued_objects.size(), 0u);
}

}  // namespace webnn::dml
