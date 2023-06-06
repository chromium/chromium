// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <d3d11.h>
#include <wrl.h>

#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/error.h"
#include "services/webnn/dml/test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_angle_util_win.h"

namespace webnn::dml {

using Microsoft::WRL::ComPtr;

const size_t kBufferSize = 16;

class WebNNCommandRecorderTest : public TestBase {
 public:
  void SetUp() override;

 protected:
  D3D12_HEAP_PROPERTIES CreateHeapProperties(D3D12_HEAP_TYPE type);
  D3D12_RESOURCE_DESC CreateResourceDesc(D3D12_RESOURCE_FLAGS flags);
  HRESULT CreateUploadBuffer(ComPtr<ID3D12Resource>& resource);
  HRESULT CreateDefaultBuffer(ComPtr<ID3D12Resource>& resource);
  HRESULT CreateReadbackBuffer(ComPtr<ID3D12Resource>& resource);
  D3D12_RESOURCE_BARRIER CreateTransitionBarrier(ID3D12Resource* resource,
                                                 D3D12_RESOURCE_STATES before,
                                                 D3D12_RESOURCE_STATES after);

  scoped_refptr<Adapter> adapter_;
};

void WebNNCommandRecorderTest::SetUp() {
  SKIP_TEST_IF(!UseGPUInTests());
  ASSERT_TRUE(InitializeGLDisplay());
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

D3D12_HEAP_PROPERTIES WebNNCommandRecorderTest::CreateHeapProperties(
    D3D12_HEAP_TYPE type) {
  return {.Type = type,
          .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
          .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
          .CreationNodeMask = 1,
          .VisibleNodeMask = 1};
}

D3D12_RESOURCE_DESC WebNNCommandRecorderTest::CreateResourceDesc(
    D3D12_RESOURCE_FLAGS flags) {
  return {.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
          .Alignment = 0,
          .Width = kBufferSize,
          .Height = 1,
          .DepthOrArraySize = 1,
          .MipLevels = 1,
          .Format = DXGI_FORMAT_UNKNOWN,
          .SampleDesc = {1, 0},
          .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
          .Flags = flags};
}

HRESULT WebNNCommandRecorderTest::CreateUploadBuffer(
    ComPtr<ID3D12Resource>& resource) {
  auto heap_properties = CreateHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
  auto resource_desc = CreateResourceDesc(D3D12_RESOURCE_FLAG_NONE);
  RETURN_IF_FAILED(adapter_->d3d12_device()->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource)));
  CHECK(resource.Get());
  return S_OK;
}

HRESULT
WebNNCommandRecorderTest::CreateDefaultBuffer(
    ComPtr<ID3D12Resource>& resource) {
  auto heap_properties = CreateHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
  auto resource_desc =
      CreateResourceDesc(D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  RETURN_IF_FAILED(adapter_->d3d12_device()->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resource)));
  CHECK(resource.Get());
  return S_OK;
}

HRESULT
WebNNCommandRecorderTest::CreateReadbackBuffer(
    ComPtr<ID3D12Resource>& resource) {
  auto heap_properties = CreateHeapProperties(D3D12_HEAP_TYPE_READBACK);
  auto resource_desc = CreateResourceDesc(D3D12_RESOURCE_FLAG_NONE);
  RETURN_IF_FAILED(adapter_->d3d12_device()->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource)));
  CHECK(resource.Get());
  return S_OK;
}

D3D12_RESOURCE_BARRIER WebNNCommandRecorderTest::CreateTransitionBarrier(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after) {
  return {.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
          .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
          .Transition = {.pResource = resource,
                         .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                         .StateBefore = before,
                         .StateAfter = after}};
}

TEST_F(WebNNCommandRecorderTest, Create) {
  EXPECT_NE(CommandRecorder::Create(adapter_), nullptr);
}

TEST_F(WebNNCommandRecorderTest, OpenCloseAndExecute) {
  auto command_recorder = CommandRecorder::Create(adapter_);
  ASSERT_NE(command_recorder.get(), nullptr);
  EXPECT_HRESULT_SUCCEEDED(command_recorder->Open());
  EXPECT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());
  EXPECT_HRESULT_SUCCEEDED(
      command_recorder->GetCommandQueue()->WaitSyncForTesting());
}

TEST_F(WebNNCommandRecorderTest, CopyBufferRegionFromUploadToDefault) {
  // Test copying data from upload buffer to default GPU buffer.
  ComPtr<ID3D12Resource> src_resource;
  ASSERT_HRESULT_SUCCEEDED(CreateUploadBuffer(src_resource));
  ComPtr<ID3D12Resource> dst_resource;
  ASSERT_HRESULT_SUCCEEDED(CreateDefaultBuffer(dst_resource));
  auto command_recorder = CommandRecorder::Create(adapter_);
  ASSERT_NE(command_recorder.get(), nullptr);
  EXPECT_HRESULT_SUCCEEDED(command_recorder->Open());
  command_recorder->ResourceBarrier({CreateTransitionBarrier(
      dst_resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
      D3D12_RESOURCE_STATE_COPY_DEST)});
  command_recorder->CopyBufferRegion(dst_resource.Get(), 0, src_resource.Get(),
                                     0, kBufferSize);
  command_recorder->ResourceBarrier({CreateTransitionBarrier(
      dst_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS)});
  EXPECT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());
  EXPECT_HRESULT_SUCCEEDED(
      command_recorder->GetCommandQueue()->WaitSyncForTesting());
}

TEST_F(WebNNCommandRecorderTest, CopyBufferRegionFromDefaultToReadback) {
  // Testing copying data from default GPU buffer to readback buffer.
  ComPtr<ID3D12Resource> src_resource;
  ASSERT_HRESULT_SUCCEEDED(CreateDefaultBuffer(src_resource));
  ComPtr<ID3D12Resource> dst_resource;
  ASSERT_HRESULT_SUCCEEDED(CreateReadbackBuffer(dst_resource));
  auto command_recorder = CommandRecorder::Create(adapter_);
  ASSERT_NE(command_recorder.get(), nullptr);
  EXPECT_HRESULT_SUCCEEDED(command_recorder->Open());
  command_recorder->ResourceBarrier({CreateTransitionBarrier(
      src_resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
      D3D12_RESOURCE_STATE_COPY_SOURCE)});
  command_recorder->CopyBufferRegion(dst_resource.Get(), 0, src_resource.Get(),
                                     0, kBufferSize);
  command_recorder->ResourceBarrier({CreateTransitionBarrier(
      src_resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS)});
  EXPECT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());
  EXPECT_HRESULT_SUCCEEDED(
      command_recorder->GetCommandQueue()->WaitSyncForTesting());
}

TEST_F(WebNNCommandRecorderTest, MultipleSubmissionsWithOneWait) {
  // Test submitting multiple command lists with one wait for GPU to complete.
  // Submit the command that copies data from upload buffer to default GPU
  // buffer.
  ComPtr<ID3D12Resource> upload_resource;
  ASSERT_HRESULT_SUCCEEDED(CreateUploadBuffer(upload_resource));
  ComPtr<ID3D12Resource> default_resource;
  ASSERT_HRESULT_SUCCEEDED(CreateDefaultBuffer(default_resource));
  auto command_recorder = CommandRecorder::Create(adapter_);
  ASSERT_NE(command_recorder.get(), nullptr);
  EXPECT_HRESULT_SUCCEEDED(command_recorder->Open());
  command_recorder->ResourceBarrier({CreateTransitionBarrier(
      default_resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
      D3D12_RESOURCE_STATE_COPY_DEST)});
  command_recorder->CopyBufferRegion(default_resource.Get(), 0,
                                     upload_resource.Get(), 0, kBufferSize);
  command_recorder->ResourceBarrier({CreateTransitionBarrier(
      default_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS)});
  EXPECT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());

  // Submit the command that copies data from default buffer to readback buffer.
  ComPtr<ID3D12Resource> readback_resource;
  ASSERT_HRESULT_SUCCEEDED(CreateReadbackBuffer(readback_resource));
  EXPECT_HRESULT_SUCCEEDED(command_recorder->Open());
  command_recorder->ResourceBarrier({CreateTransitionBarrier(
      default_resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
      D3D12_RESOURCE_STATE_COPY_SOURCE)});
  command_recorder->CopyBufferRegion(readback_resource.Get(), 0,
                                     default_resource.Get(), 0, kBufferSize);
  command_recorder->ResourceBarrier({CreateTransitionBarrier(
      default_resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS)});
  EXPECT_HRESULT_SUCCEEDED(command_recorder->CloseAndExecute());

  // Wait for GPU to complete the execution of both command lists.
  EXPECT_HRESULT_SUCCEEDED(
      command_recorder->GetCommandQueue()->WaitSyncForTesting());
}

}  // namespace webnn::dml
