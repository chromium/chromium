// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_texture_pool.h"

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/win/test_utils.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace media {
class MockD3D11Texture2D;

class MockD3D11Resource final : public IDXGIResource1 {
 public:
  MockD3D11Resource() {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void** ppvObject) override;
  ULONG STDMETHODCALLTYPE AddRef(void) override {
    return InterlockedIncrement(&refcount_);
  }

  ULONG STDMETHODCALLTYPE Release(void) override {
    ULONG refcount = InterlockedDecrement(&refcount_);
    if (refcount == 0) {
      refcount_ = 0xBAADF00D;
      delete this;
    }
    return refcount;
  }

  // IDXGIResource1
  HRESULT STDMETHODCALLTYPE
  CreateSubresourceSurface(UINT index, IDXGISurface2** ppSurface) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateSharedHandle(const SECURITY_ATTRIBUTES* pAttributes,
                     DWORD dwAccess,
                     LPCWSTR lpName,
                     HANDLE* pHandle) override;

  // IDXGIResource
  HRESULT STDMETHODCALLTYPE GetSharedHandle(HANDLE* pSharedHandle) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE GetUsage(DXGI_USAGE* pUsage) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  SetEvictionPriority(UINT eviction_priority) override;
  HRESULT STDMETHODCALLTYPE
  GetEvictionPriority(UINT* eviction_priority) override;

  // IDXGIDeviceSubObject
  HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void** ppDevice) override {
    return E_NOTIMPL;
  }

  // IDXGIObject
  HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void** ppParent) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid,
                                           UINT* pDataSize,
                                           void* pData) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid,
                                           UINT DataSize,
                                           const void* pData) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) override;

 private:
  raw_ptr<MockD3D11Texture2D> parent_;
  volatile ULONG refcount_ = 1;
};

class MockD3D11Texture2D final : public ID3D11Texture2D {
 private:
  MockD3D11Texture2D(const D3D11_TEXTURE2D_DESC* texture_description)
      : resource_(new MockD3D11Resource()) {
    memcpy(&texture_description_, texture_description,
           sizeof(D3D11_TEXTURE2D_DESC));
  }

 public:
  static HRESULT CreateInstance(const D3D11_TEXTURE2D_DESC* texture_description,
                                ID3D11Texture2D** texture2D);
  // IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void** ppvObject) override {
    if (ppvObject == nullptr) {
      return E_POINTER;
    }
    if (FAILED(QueryInterfaceInternal(riid, ppvObject))) {
      if (resource_ != nullptr)
        return resource_->QueryInterface(riid, ppvObject);
      else
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE QueryInterfaceInternal(REFIID riid,
                                                   void** ppvObject) {
    if (ppvObject == nullptr) {
      return E_POINTER;
    }

    if (riid == IID_ID3D11Texture2D || riid == IID_IUnknown) {
      *ppvObject = static_cast<ID3D11Texture2D*>(this);
    } else if (riid == IID_ID3D11Resource) {
      *ppvObject = static_cast<ID3D11Resource*>(this);
    } else if (riid == IID_ID3D11DeviceChild) {
      *ppvObject = static_cast<ID3D11DeviceChild*>(this);
    } else {
      return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
  }

  ULONG STDMETHODCALLTYPE AddRef(void) override {
    return InterlockedIncrement(&refcount_);
  }

  ULONG STDMETHODCALLTYPE Release(void) override {
    ULONG refcount = InterlockedDecrement(&refcount_);
    if (refcount == 0) {
      refcount_ = 0xBAADF00D;
      delete this;
    }
    return refcount;
  }

  // ID3D11Texture2D
  void STDMETHODCALLTYPE GetDesc(D3D11_TEXTURE2D_DESC* description) override {
    memset(description, 0, sizeof(D3D11_TEXTURE2D_DESC));
  }

  // ID3D11Resource
  void STDMETHODCALLTYPE
  GetType(D3D11_RESOURCE_DIMENSION* resource_dimension) override {
    *resource_dimension =
        D3D11_RESOURCE_DIMENSION::D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  }
  void STDMETHODCALLTYPE SetEvictionPriority(UINT eviction_priority) override {
    eviction_priority_ = eviction_priority;
  }
  UINT STDMETHODCALLTYPE GetEvictionPriority() override {
    return eviction_priority_;
  }

  // ID3D11DeviceChild
  void STDMETHODCALLTYPE GetDevice(ID3D11Device** ppDevice) override {
    device_.CopyTo(ppDevice);
  }
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid,
                                           UINT* pDataSize,
                                           void* pData) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid,
                                           UINT DataSize,
                                           const void* pData) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) override {
    // Our tests aren't checking for this right now
    return S_OK;
  }

 private:
  volatile ULONG refcount_ = 1;
  UINT eviction_priority_ = 0;
  D3D11_TEXTURE2D_DESC texture_description_;
  ComPtr<ID3D11Device> device_;
  ComPtr<IDXGIResource1> resource_;
};

class MockD3D11Device : public ID3D11Device {
 public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void** ppvObject) override {
    if (ppvObject == nullptr) {
      return E_POINTER;
    }

    if (riid == IID_ID3D11Device || riid == IID_IUnknown) {
      *ppvObject = this;
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef(void) override { return 1; }
  ULONG STDMETHODCALLTYPE Release(void) override { return 1; }

  HRESULT STDMETHODCALLTYPE
  CreateBuffer(const D3D11_BUFFER_DESC* pDesc,
               const D3D11_SUBRESOURCE_DATA* pInitialData,
               ID3D11Buffer** ppBuffer) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateTexture1D(const D3D11_TEXTURE1D_DESC* pDesc,
                  const D3D11_SUBRESOURCE_DATA* pInitialData,
                  ID3D11Texture1D** ppTexture1D) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateTexture2D(const D3D11_TEXTURE2D_DESC* pDesc,
                  const D3D11_SUBRESOURCE_DATA* pInitialData,
                  ID3D11Texture2D** ppTexture2D) override;
  HRESULT STDMETHODCALLTYPE
  CreateTexture3D(const D3D11_TEXTURE3D_DESC* pDesc,
                  const D3D11_SUBRESOURCE_DATA* pInitialData,
                  ID3D11Texture3D** ppTexture3D) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateShaderResourceView(ID3D11Resource* pResource,
                           const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc,
                           ID3D11ShaderResourceView** ppSRView) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateUnorderedAccessView(ID3D11Resource* pResource,
                            const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
                            ID3D11UnorderedAccessView** ppUAView) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateRenderTargetView(ID3D11Resource* pResource,
                         const D3D11_RENDER_TARGET_VIEW_DESC* pDesc,
                         ID3D11RenderTargetView** ppRTView) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateDepthStencilView(ID3D11Resource* pResource,
                         const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc,
                         ID3D11DepthStencilView** ppDepthStencilView) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs,
                    UINT NumElements,
                    const void* pShaderBytecodeWithInputSignature,
                    SIZE_T BytecodeLength,
                    ID3D11InputLayout** ppInputLayout) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateVertexShader(const void* pShaderBytecode,
                     SIZE_T BytecodeLength,
                     ID3D11ClassLinkage* pClassLinkage,
                     ID3D11VertexShader** ppVertexShader) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateGeometryShader(const void* pShaderBytecode,
                       SIZE_T BytecodeLength,
                       ID3D11ClassLinkage* pClassLinkage,
                       ID3D11GeometryShader** ppGeometryShader) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(
      const void* pShaderBytecode,
      SIZE_T BytecodeLength,
      const D3D11_SO_DECLARATION_ENTRY* pSODeclaration,
      UINT NumEntries,
      const UINT* pBufferStrides,
      UINT NumStrides,
      UINT RasterizedStream,
      ID3D11ClassLinkage* pClassLinkage,
      ID3D11GeometryShader** ppGeometryShader) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreatePixelShader(const void* pShaderBytecode,
                    SIZE_T BytecodeLength,
                    ID3D11ClassLinkage* pClassLinkage,
                    ID3D11PixelShader** ppPixelShader) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateHullShader(const void* pShaderBytecode,
                   SIZE_T BytecodeLength,
                   ID3D11ClassLinkage* pClassLinkage,
                   ID3D11HullShader** ppHullShader) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateDomainShader(const void* pShaderBytecode,
                     SIZE_T BytecodeLength,
                     ID3D11ClassLinkage* pClassLinkage,
                     ID3D11DomainShader** ppDomainShader) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateComputeShader(const void* pShaderBytecode,
                      SIZE_T BytecodeLength,
                      ID3D11ClassLinkage* pClassLinkage,
                      ID3D11ComputeShader** ppComputeShader) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateClassLinkage(ID3D11ClassLinkage** ppLinkage) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateBlendState(const D3D11_BLEND_DESC* pBlendStateDesc,
                   ID3D11BlendState** ppBlendState) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE CreateDepthStencilState(
      const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc,
      ID3D11DepthStencilState** ppDepthStencilState) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateRasterizerState(const D3D11_RASTERIZER_DESC* pRasterizerDesc,
                        ID3D11RasterizerState** ppRasterizerState) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateSamplerState(const D3D11_SAMPLER_DESC* pSamplerDesc,
                     ID3D11SamplerState** ppSamplerState) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE CreateQuery(const D3D11_QUERY_DESC* pQueryDesc,
                                        ID3D11Query** ppQuery) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreatePredicate(const D3D11_QUERY_DESC* pPredicateDesc,
                  ID3D11Predicate** ppPredicate) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateCounter(const D3D11_COUNTER_DESC* pCounterDesc,
                ID3D11Counter** ppCounter) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CreateDeferredContext(UINT ContextFlags,
                        ID3D11DeviceContext** ppDeferredContext) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE OpenSharedResource(HANDLE hResource,
                                               REFIID ReturnedInterface,
                                               void** ppResource) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE CheckFormatSupport(DXGI_FORMAT Format,
                                               UINT* pFormatSupport) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CheckMultisampleQualityLevels(DXGI_FORMAT Format,
                                UINT SampleCount,
                                UINT* pNumQualityLevels) override {
    return E_NOTIMPL;
  }
  void STDMETHODCALLTYPE
  CheckCounterInfo(D3D11_COUNTER_INFO* pCounterInfo) override {}
  HRESULT STDMETHODCALLTYPE CheckCounter(const D3D11_COUNTER_DESC* pDesc,
                                         D3D11_COUNTER_TYPE* pType,
                                         UINT* pActiveCounters,
                                         LPSTR szName,
                                         UINT* pNameLength,
                                         LPSTR szUnits,
                                         UINT* pUnitsLength,
                                         LPSTR szDescription,
                                         UINT* pDescriptionLength) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  CheckFeatureSupport(D3D11_FEATURE Feature,
                      void* pFeatureSupportData,
                      UINT FeatureSupportDataSize) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid,
                                           UINT* pDataSize,
                                           void* pData) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid,
                                           UINT DataSize,
                                           const void* pData) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE
  SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) override {
    return E_NOTIMPL;
  }
  D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel(void) override {
    return D3D_FEATURE_LEVEL_11_1;
  }
  UINT STDMETHODCALLTYPE GetCreationFlags(void) override { return 0; }
  HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason(void) override {
    return E_NOTIMPL;
  }
  void STDMETHODCALLTYPE
  GetImmediateContext(ID3D11DeviceContext** ppImmediateContext) override {}
  HRESULT STDMETHODCALLTYPE SetExceptionMode(UINT RaiseFlags) override {
    return E_NOTIMPL;
  }
  UINT STDMETHODCALLTYPE GetExceptionMode(void) override { return 0; }
};

// MockD3D11Resource
HRESULT STDMETHODCALLTYPE MockD3D11Resource::QueryInterface(REFIID riid,
                                                            void** ppvObject) {
  if (ppvObject == nullptr) {
    return E_POINTER;
  }

  if (riid == IID_IDXGIResource1) {
    *ppvObject = static_cast<IDXGIResource1*>(this);
  } else if (riid == IID_IDXGIResource) {
    *ppvObject = static_cast<IDXGIResource*>(this);
  } else if (riid == IID_IDXGIDeviceSubObject) {
    *ppvObject = static_cast<IDXGIDeviceSubObject*>(this);
  } else if (riid == IID_IDXGIObject) {
    *ppvObject = static_cast<IDXGIObject*>(this);
  } else if (parent_ != nullptr) {
    return parent_->QueryInterfaceInternal(riid, ppvObject);
  } else {
    return E_NOINTERFACE;
  }

  AddRef();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MockD3D11Resource::SetEvictionPriority(UINT eviction_priority) {
  parent_->SetEvictionPriority(eviction_priority);
  return S_OK;
}
HRESULT STDMETHODCALLTYPE
MockD3D11Resource::GetEvictionPriority(UINT* eviction_priority) {
  *eviction_priority = parent_->GetEvictionPriority();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MockD3D11Resource::SetPrivateDataInterface(REFGUID guid,
                                           const IUnknown* pData) {
  return parent_->SetPrivateDataInterface(guid, pData);
}

HRESULT STDMETHODCALLTYPE
MockD3D11Resource::CreateSharedHandle(const SECURITY_ATTRIBUTES* pAttributes,
                                      DWORD dwAccess,
                                      LPCWSTR lpName,
                                      HANDLE* pHandle) {
  // Using an event to create a valid nt handle
  *pHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (*pHandle == nullptr) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  return S_OK;
}

HRESULT MockD3D11Texture2D::CreateInstance(
    const D3D11_TEXTURE2D_DESC* texture_description,
    ID3D11Texture2D** texture2D) {
  MockD3D11Texture2D* mock_texture =
      new MockD3D11Texture2D(texture_description);
  if (!mock_texture) {
    return E_OUTOFMEMORY;
  }

  *texture2D = mock_texture;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MockD3D11Device::CreateTexture2D(const D3D11_TEXTURE2D_DESC* pDesc,
                                 const D3D11_SUBRESOURCE_DATA* pInitialData,
                                 ID3D11Texture2D** ppTexture2D) {
  return MockD3D11Texture2D::CreateInstance(pDesc, ppTexture2D);
}

class MediaFoundationTexturePoolTest : public testing::Test {
 public:
  MediaFoundationTexturePoolTest() {}
  base::WeakPtrFactory<MediaFoundationTexturePoolTest> weak_factory_{this};
};

TEST_F(MediaFoundationTexturePoolTest, VerifyTextureInitialization) {
  MockD3D11Device mock_d3d_device;
  media::MediaFoundationTexturePool test;
  base::WaitableEvent wait_event;
  gfx::Size frame_size(1920, 1080);

  class SpecialCallback {
   private:
    raw_ptr<base::WaitableEvent> wait_event_;
    raw_ptr<gfx::Size> frame_size_;

   public:
    SpecialCallback(base::WaitableEvent* wait_event, gfx::Size* frame_size)
        : wait_event_(wait_event), frame_size_(frame_size) {}

    void Invoke(std::vector<media::MediaFoundationFrameInfo> frame_textures,
                const gfx::Size& texture_size) {
      EXPECT_EQ(texture_size.width(), frame_size_->width());
      EXPECT_EQ(texture_size.height(), frame_size_->height());
      wait_event_->Signal();
    }
  } callback(&wait_event, &frame_size);

  EXPECT_HRESULT_SUCCEEDED(
      test.Initialize(&mock_d3d_device,
                      base::BindRepeating(&SpecialCallback::Invoke,
                                          base::Unretained(&callback)),
                      frame_size));
  wait_event.Wait();
}

}  // namespace media