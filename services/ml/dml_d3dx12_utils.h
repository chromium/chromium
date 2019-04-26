
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef SERVICES_ML_DML_D3DX12_UTILS_H_
#define SERVICES_ML_DML_D3DX12_UTILS_H_

#include "d3d12.h"

namespace ml {

struct CD3DX12_HEAP_PROPERTIES : public D3D12_HEAP_PROPERTIES {
  explicit CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE type,
                                   UINT creationNodeMask = 1,
                                   UINT nodeMask = 1) {
    Type = type;
    CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    CreationNodeMask = creationNodeMask;
    VisibleNodeMask = nodeMask;
  }
};

struct CD3DX12_RESOURCE_DESC : public D3D12_RESOURCE_DESC {
  CD3DX12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION dimension,
                        UINT64 alignment,
                        UINT64 width,
                        UINT height,
                        UINT16 depthOrArraySize,
                        UINT16 mipLevels,
                        DXGI_FORMAT format,
                        UINT sampleCount,
                        UINT sampleQuality,
                        D3D12_TEXTURE_LAYOUT layout,
                        D3D12_RESOURCE_FLAGS flags) {
    Dimension = dimension;
    Alignment = alignment;
    Width = width;
    Height = height;
    DepthOrArraySize = depthOrArraySize;
    MipLevels = mipLevels;
    Format = format;
    SampleDesc.Count = sampleCount;
    SampleDesc.Quality = sampleQuality;
    Layout = layout;
    Flags = flags;
  }
  static CD3DX12_RESOURCE_DESC Buffer(
      UINT64 width,
      D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
      UINT64 alignment = 0) {
    return CD3DX12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_BUFFER, alignment,
                                 width, 1, 1, 1, DXGI_FORMAT_UNKNOWN, 1, 0,
                                 D3D12_TEXTURE_LAYOUT_ROW_MAJOR, flags);
  }
};

struct CD3DX12_RESOURCE_BARRIER : public D3D12_RESOURCE_BARRIER {
  static CD3DX12_RESOURCE_BARRIER Transition(
      _In_ ID3D12Resource* pResource,
      D3D12_RESOURCE_STATES stateBefore,
      D3D12_RESOURCE_STATES stateAfter,
      UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
      D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE) {
    CD3DX12_RESOURCE_BARRIER result = {};
    D3D12_RESOURCE_BARRIER& barrier = result;
    result.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    result.Flags = flags;
    barrier.Transition.pResource = pResource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = subresource;
    return result;
  }
  static inline CD3DX12_RESOURCE_BARRIER UAV(_In_ ID3D12Resource* pResource) {
    CD3DX12_RESOURCE_BARRIER result = {};
    D3D12_RESOURCE_BARRIER& barrier = result;
    result.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = pResource;
    return result;
  }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_TEXTURE_COPY_LOCATION : public D3D12_TEXTURE_COPY_LOCATION {
  CD3DX12_TEXTURE_COPY_LOCATION() = default;
  explicit CD3DX12_TEXTURE_COPY_LOCATION(const D3D12_TEXTURE_COPY_LOCATION& o)
      : D3D12_TEXTURE_COPY_LOCATION(o) {}
  CD3DX12_TEXTURE_COPY_LOCATION(_In_ ID3D12Resource* pRes) {
    pResource = pRes;
    Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    PlacedFootprint = {};
  }
  CD3DX12_TEXTURE_COPY_LOCATION(
      _In_ ID3D12Resource* pRes,
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT const& Footprint) {
    pResource = pRes;
    Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    PlacedFootprint = Footprint;
  }
  CD3DX12_TEXTURE_COPY_LOCATION(_In_ ID3D12Resource* pRes, UINT Sub) {
    pResource = pRes;
    Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    SubresourceIndex = Sub;
  }
};

//------------------------------------------------------------------------------------------------
// Row-by-row memcpy
inline void MemcpySubresource(_In_ const D3D12_MEMCPY_DEST* pDest,
                              _In_ const D3D12_SUBRESOURCE_DATA* pSrc,
                              SIZE_T RowSizeInBytes,
                              UINT NumRows,
                              UINT NumSlices) {
  for (UINT z = 0; z < NumSlices; ++z) {
    BYTE* pDestSlice =
        reinterpret_cast<BYTE*>(pDest->pData) + pDest->SlicePitch * z;
    const BYTE* pSrcSlice =
        reinterpret_cast<const BYTE*>(pSrc->pData) + pSrc->SlicePitch * z;
    for (UINT y = 0; y < NumRows; ++y) {
      memcpy(pDestSlice + pDest->RowPitch * y, pSrcSlice + pSrc->RowPitch * y,
             RowSizeInBytes);
    }
  }
}

// All arrays must be populated (e.g. by calling GetCopyableFootprints)
inline UINT64 UpdateSubresources(
    _In_ ID3D12GraphicsCommandList* pCmdList,
    _In_ ID3D12Resource* pDestinationResource,
    _In_ ID3D12Resource* pIntermediate,
    _In_range_(0, D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
    _In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource)
        UINT NumSubresources,
    UINT64 RequiredSize,
    _In_reads_(NumSubresources)
        const D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts,
    _In_reads_(NumSubresources) const UINT* pNumRows,
    _In_reads_(NumSubresources) const UINT64* pRowSizesInBytes,
    _In_reads_(NumSubresources) const D3D12_SUBRESOURCE_DATA* pSrcData) {
  // Minor validation
  auto IntermediateDesc = pIntermediate->GetDesc();
  auto DestinationDesc = pDestinationResource->GetDesc();
  if (IntermediateDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER ||
      IntermediateDesc.Width < RequiredSize + pLayouts[0].Offset ||
      RequiredSize > SIZE_T(-1) ||
      (DestinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER &&
       (FirstSubresource != 0 || NumSubresources != 1))) {
    return 0;
  }

  BYTE* pData;
  HRESULT hr = pIntermediate->Map(0, nullptr, reinterpret_cast<void**>(&pData));
  if (FAILED(hr)) {
    return 0;
  }

  for (UINT i = 0; i < NumSubresources; ++i) {
    if (pRowSizesInBytes[i] > SIZE_T(-1))
      return 0;
    D3D12_MEMCPY_DEST DestData = {
        pData + pLayouts[i].Offset, pLayouts[i].Footprint.RowPitch,
        SIZE_T(pLayouts[i].Footprint.RowPitch) * SIZE_T(pNumRows[i])};
    MemcpySubresource(&DestData, &pSrcData[i],
                      static_cast<SIZE_T>(pRowSizesInBytes[i]), pNumRows[i],
                      pLayouts[i].Footprint.Depth);
  }
  pIntermediate->Unmap(0, nullptr);

  if (DestinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
    pCmdList->CopyBufferRegion(pDestinationResource, 0, pIntermediate,
                               pLayouts[0].Offset, pLayouts[0].Footprint.Width);
  } else {
    for (UINT i = 0; i < NumSubresources; ++i) {
      CD3DX12_TEXTURE_COPY_LOCATION Dst(pDestinationResource,
                                        i + FirstSubresource);
      CD3DX12_TEXTURE_COPY_LOCATION Src(pIntermediate, pLayouts[i]);
      pCmdList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
    }
  }
  return RequiredSize;
}

// Heap-allocating UpdateSubresources implementation
inline UINT64 UpdateSubresources(
    _In_ ID3D12GraphicsCommandList* pCmdList,
    _In_ ID3D12Resource* pDestinationResource,
    _In_ ID3D12Resource* pIntermediate,
    UINT64 IntermediateOffset,
    _In_range_(0, D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
    _In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource)
        UINT NumSubresources,
    _In_reads_(NumSubresources) D3D12_SUBRESOURCE_DATA* pSrcData) {
  UINT64 RequiredSize = 0;
  UINT64 MemToAlloc =
      static_cast<UINT64>(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) +
                          sizeof(UINT) + sizeof(UINT64)) *
      NumSubresources;
  if (MemToAlloc > SIZE_MAX) {
    return 0;
  }
  void* pMem = HeapAlloc(GetProcessHeap(), 0, static_cast<SIZE_T>(MemToAlloc));
  if (pMem == nullptr) {
    return 0;
  }
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts =
      reinterpret_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(pMem);
  UINT64* pRowSizesInBytes =
      reinterpret_cast<UINT64*>(pLayouts + NumSubresources);
  UINT* pNumRows = reinterpret_cast<UINT*>(pRowSizesInBytes + NumSubresources);

  auto Desc = pDestinationResource->GetDesc();
  ID3D12Device* pDevice = nullptr;
  pDestinationResource->GetDevice(__uuidof(*pDevice),
                                  reinterpret_cast<void**>(&pDevice));
  pDevice->GetCopyableFootprints(&Desc, FirstSubresource, NumSubresources,
                                 IntermediateOffset, pLayouts, pNumRows,
                                 pRowSizesInBytes, &RequiredSize);
  pDevice->Release();

  UINT64 Result =
      UpdateSubresources(pCmdList, pDestinationResource, pIntermediate,
                         FirstSubresource, NumSubresources, RequiredSize,
                         pLayouts, pNumRows, pRowSizesInBytes, pSrcData);
  HeapFree(GetProcessHeap(), 0, pMem);
  return Result;
}

//------------------------------------------------------------------------------------------------
// Stack-allocating UpdateSubresources implementation
template <UINT MaxSubresources>
inline UINT64 UpdateSubresources(
    _In_ ID3D12GraphicsCommandList* pCmdList,
    _In_ ID3D12Resource* pDestinationResource,
    _In_ ID3D12Resource* pIntermediate,
    UINT64 IntermediateOffset,
    _In_range_(0, MaxSubresources) UINT FirstSubresource,
    _In_range_(1, MaxSubresources - FirstSubresource) UINT NumSubresources,
    _In_reads_(NumSubresources) D3D12_SUBRESOURCE_DATA* pSrcData) {
  UINT64 RequiredSize = 0;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layouts[MaxSubresources];
  UINT NumRows[MaxSubresources];
  UINT64 RowSizesInBytes[MaxSubresources];

  auto Desc = pDestinationResource->GetDesc();
  ID3D12Device* pDevice = nullptr;
  pDestinationResource->GetDevice(__uuidof(*pDevice),
                                  reinterpret_cast<void**>(&pDevice));
  pDevice->GetCopyableFootprints(&Desc, FirstSubresource, NumSubresources,
                                 IntermediateOffset, Layouts, NumRows,
                                 RowSizesInBytes, &RequiredSize);
  pDevice->Release();

  return UpdateSubresources(pCmdList, pDestinationResource, pIntermediate,
                            FirstSubresource, NumSubresources, RequiredSize,
                            Layouts, NumRows, RowSizesInBytes, pSrcData);
}

}  // namespace ml

#endif  // SERVICES_ML_DML_D3DX12_UTILS_H_
