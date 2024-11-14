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

  MOCK_STDCALL_METHOD2(QueryInterface, HRESULT(REFIID riid, void** ppv));

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

class D3D12GraphicsCommandListMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D12GraphicsCommandList> {
 public:
  D3D12GraphicsCommandListMock();
  ~D3D12GraphicsCommandListMock() override;

  // Interfaces of ID3D12Object

  MOCK_STDCALL_METHOD3(GetPrivateData,
                       HRESULT(REFGUID guid, UINT* pDataSize, void* pData));
  MOCK_STDCALL_METHOD3(SetPrivateData,
                       HRESULT(REFGUID guid, UINT dataSize, const void* pData));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(REFGUID guid, const IUnknown* pData));
  MOCK_STDCALL_METHOD1(SetName, HRESULT(LPCWSTR Name));

  // Interface of ID3D12DeviceChild

  MOCK_STDCALL_METHOD2(GetDevice, HRESULT(REFIID riid, void** ppvDevice));

  // Interface of ID3D12CommandList

  MOCK_STDCALL_METHOD0(GetType, D3D12_COMMAND_LIST_TYPE());

  // Interfaces of ID3D12GraphicsCommandList

  MOCK_STDCALL_METHOD0(Close, HRESULT());
  MOCK_STDCALL_METHOD2(Reset,
                       HRESULT(ID3D12CommandAllocator* pAllocator,
                               ID3D12PipelineState* pInitialState));
  MOCK_STDCALL_METHOD1(ClearState, void(ID3D12PipelineState* pPipelineState));
  MOCK_STDCALL_METHOD4(DrawInstanced,
                       void(UINT VertexCountPerInstance,
                            UINT InstanceCount,
                            UINT StartVertexLocation,
                            UINT StartInstanceLocation));
  MOCK_STDCALL_METHOD5(DrawIndexedInstanced,
                       void(UINT IndexCountPerInstance,
                            UINT InstanceCount,
                            UINT StartIndexLocation,
                            INT BaseVertexLocation,
                            UINT StartInstanceLocation));
  MOCK_STDCALL_METHOD3(Dispatch,
                       void(UINT ThreadGroupCountX,
                            UINT ThreadGroupCountY,
                            UINT ThreadGroupCountZ));
  MOCK_STDCALL_METHOD5(CopyBufferRegion,
                       void(ID3D12Resource* pDstBuffer,
                            UINT64 DstOffset,
                            ID3D12Resource* pSrcBuffer,
                            UINT64 SrcOffset,
                            UINT64 NumBytes));
  MOCK_STDCALL_METHOD6(CopyTextureRegion,
                       void(const D3D12_TEXTURE_COPY_LOCATION* pDst,
                            UINT DstX,
                            UINT DstY,
                            UINT DstZ,
                            const D3D12_TEXTURE_COPY_LOCATION* pSrc,
                            const D3D12_BOX* pSrcBox));
  MOCK_STDCALL_METHOD2(CopyResource,
                       void(ID3D12Resource* pDstResource,
                            ID3D12Resource* pSrcResource));
  MOCK_STDCALL_METHOD6(
      CopyTiles,
      void(ID3D12Resource* pTiledResource,
           const D3D12_TILED_RESOURCE_COORDINATE* pTileRegionStartCoordinate,
           const D3D12_TILE_REGION_SIZE* pTileRegionSize,
           ID3D12Resource* pBuffer,
           UINT64 BufferStartOffsetInBytes,
           D3D12_TILE_COPY_FLAGS Flags));
  MOCK_STDCALL_METHOD5(ResolveSubresource,
                       void(ID3D12Resource* pDstResource,
                            UINT DstSubresource,
                            ID3D12Resource* pSrcResource,
                            UINT SrcSubresource,
                            DXGI_FORMAT Format));
  MOCK_STDCALL_METHOD1(IASetPrimitiveTopology,
                       void(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology));
  MOCK_STDCALL_METHOD2(RSSetViewports,
                       void(UINT NumViewports,
                            const D3D12_VIEWPORT* pViewports));
  MOCK_STDCALL_METHOD2(RSSetScissorRects,
                       void(UINT NumRects, const D3D12_RECT* pRects));
  MOCK_STDCALL_METHOD1(OMSetBlendFactor, void(const FLOAT BlendFactor[4]));
  MOCK_STDCALL_METHOD1(OMSetStencilRef, void(UINT StencilRef));
  MOCK_STDCALL_METHOD1(SetPipelineState,
                       void(ID3D12PipelineState* pPipelineState));
  MOCK_STDCALL_METHOD2(ResourceBarrier,
                       void(UINT NumBarriers,
                            const D3D12_RESOURCE_BARRIER* pBarriers));
  MOCK_STDCALL_METHOD1(ExecuteBundle,
                       void(ID3D12GraphicsCommandList* pCommandList));
  MOCK_STDCALL_METHOD2(SetDescriptorHeaps,
                       void(UINT NumDescriptorHeaps,
                            ID3D12DescriptorHeap* const* ppDescriptorHeaps));
  MOCK_STDCALL_METHOD1(SetComputeRootSignature,
                       void(ID3D12RootSignature* pRootSignature));
  MOCK_STDCALL_METHOD1(SetGraphicsRootSignature,
                       void(ID3D12RootSignature* pRootSignature));
  MOCK_STDCALL_METHOD2(SetComputeRootDescriptorTable,
                       void(UINT RootParameterIndex,
                            D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor));
  MOCK_STDCALL_METHOD2(SetGraphicsRootDescriptorTable,
                       void(UINT RootParameterIndex,
                            D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor));
  MOCK_STDCALL_METHOD3(SetComputeRoot32BitConstant,
                       void(UINT RootParameterIndex,
                            UINT SrcData,
                            UINT DestOffsetIn32BitValues));
  MOCK_STDCALL_METHOD3(SetGraphicsRoot32BitConstant,
                       void(UINT RootParameterIndex,
                            UINT SrcData,
                            UINT DestOffsetIn32BitValues));
  MOCK_STDCALL_METHOD4(SetComputeRoot32BitConstants,
                       void(UINT RootParameterIndex,
                            UINT Num32BitValuesToSet,
                            const void* pSrcData,
                            UINT DestOffsetIn32BitValues));
  MOCK_STDCALL_METHOD4(SetGraphicsRoot32BitConstants,
                       void(UINT RootParameterIndex,
                            UINT Num32BitValuesToSet,
                            const void* pSrcData,
                            UINT DestOffsetIn32BitValues));
  MOCK_STDCALL_METHOD2(SetComputeRootConstantBufferView,
                       void(UINT RootParameterIndex,
                            D3D12_GPU_VIRTUAL_ADDRESS BufferLocation));
  MOCK_STDCALL_METHOD2(SetGraphicsRootConstantBufferView,
                       void(UINT RootParameterIndex,
                            D3D12_GPU_VIRTUAL_ADDRESS BufferLocation));
  MOCK_STDCALL_METHOD2(SetComputeRootShaderResourceView,
                       void(UINT RootParameterIndex,
                            D3D12_GPU_VIRTUAL_ADDRESS BufferLocation));
  MOCK_STDCALL_METHOD2(SetGraphicsRootShaderResourceView,
                       void(UINT RootParameterIndex,
                            D3D12_GPU_VIRTUAL_ADDRESS BufferLocation));
  MOCK_STDCALL_METHOD2(SetComputeRootUnorderedAccessView,
                       void(UINT RootParameterIndex,
                            D3D12_GPU_VIRTUAL_ADDRESS BufferLocation));
  MOCK_STDCALL_METHOD2(SetGraphicsRootUnorderedAccessView,
                       void(UINT RootParameterIndex,
                            D3D12_GPU_VIRTUAL_ADDRESS BufferLocation));
  MOCK_STDCALL_METHOD1(IASetIndexBuffer,
                       void(const D3D12_INDEX_BUFFER_VIEW* pView));
  MOCK_STDCALL_METHOD3(IASetVertexBuffers,
                       void(UINT StartSlot,
                            UINT NumViews,
                            const D3D12_VERTEX_BUFFER_VIEW* pViews));
  MOCK_STDCALL_METHOD3(SOSetTargets,
                       void(UINT StartSlot,
                            UINT NumViews,
                            const D3D12_STREAM_OUTPUT_BUFFER_VIEW* pViews));
  MOCK_STDCALL_METHOD4(
      OMSetRenderTargets,
      void(UINT NumRenderTargetDescriptors,
           const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors,
           BOOL RTsSingleHandleToDescriptorRange,
           const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor));
  MOCK_STDCALL_METHOD6(ClearDepthStencilView,
                       void(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView,
                            D3D12_CLEAR_FLAGS ClearFlags,
                            FLOAT Depth,
                            UINT8 Stencil,
                            UINT NumRects,
                            const D3D12_RECT* pRects));
  MOCK_STDCALL_METHOD4(ClearRenderTargetView,
                       void(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView,
                            const FLOAT ColorRGBA[4],
                            UINT NumRects,
                            const D3D12_RECT* pRects));
  MOCK_STDCALL_METHOD6(
      ClearUnorderedAccessViewUint,
      void(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
           D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
           ID3D12Resource* pResource,
           const UINT Values[4],
           UINT NumRects,
           const D3D12_RECT* pRects));
  MOCK_STDCALL_METHOD6(
      ClearUnorderedAccessViewFloat,
      void(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
           D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
           ID3D12Resource* pResource,
           const FLOAT Values[4],
           UINT NumRects,
           const D3D12_RECT* pRects));
  MOCK_STDCALL_METHOD2(DiscardResource,
                       void(ID3D12Resource* pResource,
                            const D3D12_DISCARD_REGION* pRegion));
  MOCK_STDCALL_METHOD3(BeginQuery,
                       void(ID3D12QueryHeap* pQueryHeap,
                            D3D12_QUERY_TYPE Type,
                            UINT Index));
  MOCK_STDCALL_METHOD3(EndQuery,
                       void(ID3D12QueryHeap* pQueryHeap,
                            D3D12_QUERY_TYPE Type,
                            UINT Index));
  MOCK_STDCALL_METHOD6(ResolveQueryData,
                       void(ID3D12QueryHeap* pQueryHeap,
                            D3D12_QUERY_TYPE Type,
                            UINT StartIndex,
                            UINT NumQueries,
                            ID3D12Resource* pDestinationBuffer,
                            UINT64 AlignedDestinationBufferOffset));
  MOCK_STDCALL_METHOD3(SetPredication,
                       void(ID3D12Resource* pBuffer,
                            UINT64 AlignedBufferOffset,
                            D3D12_PREDICATION_OP Operation));
  MOCK_STDCALL_METHOD3(SetMarker,
                       void(UINT Metadata, const void* pData, UINT Size));
  MOCK_STDCALL_METHOD3(BeginEvent,
                       void(UINT Metadata, const void* pData, UINT Size));
  MOCK_STDCALL_METHOD0(EndEvent, void());
  MOCK_STDCALL_METHOD6(ExecuteIndirect,
                       void(ID3D12CommandSignature* pCommandSignature,
                            UINT MaxCommandCount,
                            ID3D12Resource* pArgumentBuffer,
                            UINT64 ArgumentBufferOffset,
                            ID3D12Resource* pCountBuffer,
                            UINT64 CountBufferOffset));
};

}  // namespace media

#endif  // MEDIA_BASE_WIN_D3D12_MOCKS_H_
