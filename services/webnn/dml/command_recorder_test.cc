// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <d3d11.h>
#include <wrl.h>

#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_angle_util_win.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

class WebNNCommandRecorderTest : public TestBase {
 public:
  void SetUp() override;

 protected:
  scoped_refptr<Adapter> adapter_;
};

void WebNNCommandRecorderTest::SetUp() {
  TestBase::SetUp();
  // Skip all tests for this fixture.
  if (!display_) {
    return;
  }

  ComPtr<ID3D11Device> d3d11_device = gl::QueryD3D11DeviceObjectFromANGLE();
  ASSERT_NE(d3d11_device.Get(), nullptr);
  ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device.As(&dxgi_device);
  ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_device->GetAdapter(&dxgi_adapter);
  ASSERT_NE(dxgi_adapter.Get(), nullptr);
  adapter_ = Adapter::Create(dxgi_adapter);
  ASSERT_NE(adapter_.get(), nullptr);
}

TEST_F(WebNNCommandRecorderTest, CreateCommandRecoder) {
  EXPECT_NE(CommandRecorder::Create(adapter_), nullptr);
}

TEST_F(WebNNCommandRecorderTest, CopyBufferRegionFromCPUToGPU) {
  D3D12_HEAP_PROPERTIES heap_properties;
  heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
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
  resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

  ComPtr<ID3D12Resource> src_resource;
  ASSERT_EQ(adapter_->d3d12_device()->CreateCommittedResource(
                &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(&src_resource)),
            S_OK);
  ASSERT_NE(src_resource.Get(), nullptr);

  ComPtr<ID3D12Resource> dest_resource;
  heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
  resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  ASSERT_EQ(adapter_->d3d12_device()->CreateCommittedResource(
                &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                IID_PPV_ARGS(&dest_resource)),
            S_OK);
  ASSERT_NE(dest_resource.Get(), nullptr);

  D3D12_RESOURCE_BARRIER transition_barrier;
  transition_barrier.Transition.pResource = dest_resource.Get();
  transition_barrier.Transition.StateBefore =
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  transition_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
  transition_barrier.Transition.Subresource =
      D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  transition_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  transition_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

  auto command_recorder = CommandRecorder::Create(adapter_);
  ASSERT_NE(command_recorder.get(), nullptr);
  command_recorder->ResourceBarrier({transition_barrier});
  command_recorder->CopyBufferRegion(dest_resource.Get(), 0, src_resource.Get(),
                                     0, 16);
  transition_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  transition_barrier.Transition.StateAfter =
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  command_recorder->ResourceBarrier({transition_barrier});
  EXPECT_EQ(command_recorder->CloseAndExecute(), S_OK);
}

TEST_F(WebNNCommandRecorderTest, CopyBufferRegionFromGPUToCPU) {
  D3D12_HEAP_PROPERTIES heap_properties;
  heap_properties.Type = D3D12_HEAP_TYPE_READBACK;
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
  resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

  ComPtr<ID3D12Resource> dest_resource;
  ASSERT_EQ(adapter_->d3d12_device()->CreateCommittedResource(
                &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                IID_PPV_ARGS(&dest_resource)),
            S_OK);
  ASSERT_NE(dest_resource.Get(), nullptr);

  ComPtr<ID3D12Resource> src_resource;
  heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
  resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  ASSERT_EQ(adapter_->d3d12_device()->CreateCommittedResource(
                &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                IID_PPV_ARGS(&src_resource)),
            S_OK);
  ASSERT_NE(src_resource.Get(), nullptr);

  auto command_recorder = CommandRecorder::Create(adapter_);
  ASSERT_NE(command_recorder.get(), nullptr);
  D3D12_RESOURCE_BARRIER transition_barrier;
  transition_barrier.Transition.pResource = src_resource.Get();
  transition_barrier.Transition.StateBefore =
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  transition_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
  transition_barrier.Transition.Subresource =
      D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  transition_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  transition_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  command_recorder->ResourceBarrier({transition_barrier});
  command_recorder->CopyBufferRegion(dest_resource.Get(), 0, src_resource.Get(),
                                     0, 16);
  transition_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
  transition_barrier.Transition.StateAfter =
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  command_recorder->ResourceBarrier({transition_barrier});
  EXPECT_EQ(command_recorder->CloseAndExecute(), S_OK);
}

}  // namespace webnn::dml
