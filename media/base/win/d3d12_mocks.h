// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_D3D12_MOCKS_H_
#define MEDIA_BASE_WIN_D3D12_MOCKS_H_

#include <d3d12.h>
#include <wrl.h>

#include "media/base/win/test_utils.h"

namespace media {

class D3D12DeviceMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D12Device> {
 public:
  D3D12DeviceMock();
  ~D3D12DeviceMock() override;
  MOCK_STDCALL_METHOD3(GetPrivateData, HRESULT(REFGUID, UINT*, void*));
  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(REFGUID, UINT, const void*));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(REFGUID, const IUnknown*));
  MOCK_STDCALL_METHOD1(SetName, HRESULT(LPCWSTR));
  MOCK_STDCALL_METHOD0(GetNodeCount, UINT(void));
  MOCK_STDCALL_METHOD3(CreateCommandQueue,
                       HRESULT(const D3D12_COMMAND_QUEUE_DESC*,
                               REFIID,
                               void**));
  MOCK_STDCALL_METHOD3(CreateCommandAllocator,
                       HRESULT(D3D12_COMMAND_LIST_TYPE, REFIID, void**));
  MOCK_STDCALL_METHOD3(CreateGraphicsPipelineState,
                       HRESULT(const D3D12_GRAPHICS_PIPELINE_STATE_DESC*,
                               REFIID,
                               void**));
  MOCK_STDCALL_METHOD3(CreateComputePipelineState,
                       HRESULT(const D3D12_COMPUTE_PIPELINE_STATE_DESC*,
                               REFIID,
                               void**));
  MOCK_STDCALL_METHOD6(CreateCommandList,
                       HRESULT(UINT,
                               D3D12_COMMAND_LIST_TYPE,
                               ID3D12CommandAllocator*,
                               ID3D12PipelineState*,
                               REFIID,
                               void**));
  MOCK_STDCALL_METHOD3(CheckFeatureSupport,
                       HRESULT(D3D12_FEATURE Feature,
                               void* pFeatureSupportData,
                               UINT FeatureSupportDataSize));
  MOCK_STDCALL_METHOD3(CreateDescriptorHeap,
                       HRESULT(const D3D12_DESCRIPTOR_HEAP_DESC*,
                               REFIID,
                               void**));
  MOCK_STDCALL_METHOD5(CreateRootSignature,
                       HRESULT(UINT, const void*, SIZE_T, REFIID, void**));
  MOCK_STDCALL_METHOD2(CreateConstantBufferView,
                       void(const D3D12_CONSTANT_BUFFER_VIEW_DESC*,
                            D3D12_CPU_DESCRIPTOR_HANDLE));
  MOCK_STDCALL_METHOD3(CreateShaderResourceView,
                       void(ID3D12Resource*,
                            const D3D12_SHADER_RESOURCE_VIEW_DESC*,
                            D3D12_CPU_DESCRIPTOR_HANDLE));
  MOCK_STDCALL_METHOD4(CreateUnorderedAccessView,
                       void(ID3D12Resource*,
                            ID3D12Resource*,
                            const D3D12_UNORDERED_ACCESS_VIEW_DESC*,
                            D3D12_CPU_DESCRIPTOR_HANDLE));
  MOCK_STDCALL_METHOD3(CreateRenderTargetView,
                       void(ID3D12Resource*,
                            const D3D12_RENDER_TARGET_VIEW_DESC*,
                            D3D12_CPU_DESCRIPTOR_HANDLE));
  MOCK_STDCALL_METHOD3(CreateDepthStencilView,
                       void(ID3D12Resource*,
                            const D3D12_DEPTH_STENCIL_VIEW_DESC*,
                            D3D12_CPU_DESCRIPTOR_HANDLE));
  MOCK_STDCALL_METHOD2(CreateSampler,
                       void(const D3D12_SAMPLER_DESC*,
                            D3D12_CPU_DESCRIPTOR_HANDLE));
  MOCK_STDCALL_METHOD7(CopyDescriptors,
                       void(UINT,
                            const D3D12_CPU_DESCRIPTOR_HANDLE*,
                            const UINT*,
                            UINT,
                            const D3D12_CPU_DESCRIPTOR_HANDLE*,
                            const UINT*,
                            D3D12_DESCRIPTOR_HEAP_TYPE));
  MOCK_STDCALL_METHOD4(CopyDescriptorsSimple,
                       void(UINT,
                            D3D12_CPU_DESCRIPTOR_HANDLE,
                            D3D12_CPU_DESCRIPTOR_HANDLE,
                            D3D12_DESCRIPTOR_HEAP_TYPE));
  MOCK_STDCALL_METHOD3(
      GetResourceAllocationInfo,
      D3D12_RESOURCE_ALLOCATION_INFO(UINT, UINT, const D3D12_RESOURCE_DESC*));
  MOCK_STDCALL_METHOD2(GetCustomHeapProperties,
                       D3D12_HEAP_PROPERTIES(UINT, D3D12_HEAP_TYPE));
  MOCK_STDCALL_METHOD7(CreateCommittedResource,
                       HRESULT(const D3D12_HEAP_PROPERTIES*,
                               D3D12_HEAP_FLAGS,
                               const D3D12_RESOURCE_DESC*,
                               D3D12_RESOURCE_STATES,
                               const D3D12_CLEAR_VALUE*,
                               REFIID,
                               void**));
  MOCK_STDCALL_METHOD3(CreateHeap,
                       HRESULT(const D3D12_HEAP_DESC*, REFIID, void**));
  MOCK_STDCALL_METHOD7(CreatePlacedResource,
                       HRESULT(ID3D12Heap*,
                               UINT64,
                               const D3D12_RESOURCE_DESC*,
                               D3D12_RESOURCE_STATES,
                               const D3D12_CLEAR_VALUE*,
                               REFIID,
                               void**));
  MOCK_STDCALL_METHOD5(CreateReservedResource,
                       HRESULT(const D3D12_RESOURCE_DESC*,
                               D3D12_RESOURCE_STATES,
                               const D3D12_CLEAR_VALUE*,
                               REFIID,
                               void**));
  MOCK_STDCALL_METHOD5(CreateSharedHandle,
                       HRESULT(ID3D12DeviceChild*,
                               const SECURITY_ATTRIBUTES*,
                               DWORD,
                               LPCWSTR,
                               HANDLE*));
  MOCK_STDCALL_METHOD3(OpenSharedHandle, HRESULT(HANDLE, REFIID, void**));
  MOCK_STDCALL_METHOD3(OpenSharedHandleByName,
                       HRESULT(LPCWSTR, DWORD, HANDLE*));
  MOCK_STDCALL_METHOD2(MakeResident, HRESULT(UINT, ID3D12Pageable* const*));
  MOCK_STDCALL_METHOD2(Evict, HRESULT(UINT, ID3D12Pageable* const*));
  MOCK_STDCALL_METHOD4(CreateFence,
                       HRESULT(UINT64, D3D12_FENCE_FLAGS, REFIID, void**));
  MOCK_STDCALL_METHOD0(GetDeviceRemovedReason, HRESULT());
  MOCK_STDCALL_METHOD8(GetCopyableFootprints,
                       void(const D3D12_RESOURCE_DESC*,
                            UINT,
                            UINT,
                            UINT64,
                            D3D12_PLACED_SUBRESOURCE_FOOTPRINT*,
                            UINT*,
                            UINT64*,
                            UINT64*));
  MOCK_STDCALL_METHOD3(CreateQueryHeap,
                       HRESULT(const D3D12_QUERY_HEAP_DESC*, REFIID, void**));
  MOCK_STDCALL_METHOD1(SetStablePowerState, HRESULT(BOOL));
  MOCK_STDCALL_METHOD4(CreateCommandSignature,
                       HRESULT(const D3D12_COMMAND_SIGNATURE_DESC*,
                               ID3D12RootSignature*,
                               REFIID,
                               void**));
  MOCK_STDCALL_METHOD7(GetResourceTiling,
                       void(ID3D12Resource*,
                            UINT*,
                            D3D12_PACKED_MIP_INFO*,
                            D3D12_TILE_SHAPE*,
                            UINT*,
                            UINT,
                            D3D12_SUBRESOURCE_TILING*));
  MOCK_STDCALL_METHOD0(GetAdapterLuid, LUID(void));
  MOCK_STDCALL_METHOD1(GetDescriptorHandleIncrementSize,
                       UINT(D3D12_DESCRIPTOR_HEAP_TYPE));
};

class D3D12ResourceMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D12Resource> {
 public:
  D3D12ResourceMock();
  ~D3D12ResourceMock() override;
  MOCK_STDCALL_METHOD3(GetPrivateData, HRESULT(REFGUID, UINT*, void*));
  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(REFGUID, UINT, const void*));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(REFGUID, const IUnknown*));
  MOCK_STDCALL_METHOD1(SetName, HRESULT(LPCWSTR));
  MOCK_STDCALL_METHOD2(GetDevice, HRESULT(REFIID, void**));
  MOCK_STDCALL_METHOD0(GetDesc, D3D12_RESOURCE_DESC(void));
  MOCK_STDCALL_METHOD0(GetGPUVirtualAddress, D3D12_GPU_VIRTUAL_ADDRESS(void));
  MOCK_STDCALL_METHOD3(Map, HRESULT(UINT, const D3D12_RANGE*, void**));
  MOCK_STDCALL_METHOD2(Unmap, void(UINT, const D3D12_RANGE*));
  MOCK_STDCALL_METHOD2(GetHeapProperties,
                       HRESULT(D3D12_HEAP_PROPERTIES*, D3D12_HEAP_FLAGS*));
  MOCK_STDCALL_METHOD5(
      WriteToSubresource,
      HRESULT(UINT, const D3D12_BOX*, const void*, UINT, UINT));
  MOCK_STDCALL_METHOD5(ReadFromSubresource,
                       HRESULT(void*, UINT, UINT, UINT, const D3D12_BOX*));
};

}  // namespace media

#endif  // MEDIA_BASE_WIN_D3D12_MOCKS_H_
