// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_D3D11_MOCKS_H_
#define MEDIA_BASE_WIN_D3D11_MOCKS_H_

#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "media/base/win/d3d11_create_device_cb.h"
#include "testing/gmock/include/gmock/gmock.h"

#define MOCK_STDCALL_METHOD0(Name, Types) \
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD1(Name, Types) \
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD2(Name, Types) \
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD3(Name, Types) \
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD4(Name, Types) \
  MOCK_METHOD4_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD5(Name, Types) \
  MOCK_METHOD5_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD6(Name, Types) \
  MOCK_METHOD6_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD7(Name, Types) \
  MOCK_METHOD7_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD8(Name, Types) \
  MOCK_METHOD8_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD9(Name, Types) \
  MOCK_METHOD9_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

// Helper ON_CALL and EXPECT_CALL for Microsoft::WRL::ComPtr, e.g.
//   COM_EXPECT_CALL(foo_, Bar());
// where |foo_| is ComPtr<D3D11FooMock>.
#define COM_ON_CALL(obj, call) ON_CALL(*obj.Get(), call)
#define COM_EXPECT_CALL(obj, call) EXPECT_CALL(*obj.Get(), call)

namespace media {

// Use this action when using SetArgPointee with COM pointers.
// e.g.
// COM_EXPECT_CALL(device_mock_, QueryInterface(IID_ID3D11VideoDevice, _))
//  .WillRepeatedly(DoAll(
//     SetComPointee<1>(video_device_mock_.Get()), Return(S_OK)));
ACTION_TEMPLATE(SetComPointee,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p)) {
  p->AddRef();
  *std::get<k>(args) = p;
}

// Same as above, but returns S_OK for convenience.
// e.g.
// COM_EXPECT_CALL(device_mock_, QueryInterface(IID_ID3D11VideoDevice, _))
//  .WillRepeatedly(SetComPointeeAndReturnOk<1>(video_device_mock_.Get()));
ACTION_TEMPLATE(SetComPointeeAndReturnOk,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p)) {
  p->AddRef();
  *std::get<k>(args) = p;
  return S_OK;
}

// Use this function to create a mock so that they are ref-counted correctly.
template <typename Interface>
Microsoft::WRL::ComPtr<Interface> CreateD3D11Mock() {
  return Microsoft::WRL::Make<Interface>();
}

// Class for mocking D3D11CreateDevice() function.
class D3D11CreateDeviceMock {
 public:
  D3D11CreateDeviceMock();
  ~D3D11CreateDeviceMock();
  MOCK_METHOD10(Create, D3D11CreateDeviceCB::RunType);
};

class D3D11Texture2DMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D11Texture2D> {
 public:
  D3D11Texture2DMock();
  ~D3D11Texture2DMock() override;
  MOCK_STDCALL_METHOD1(GetDevice, void(ID3D11Device**));
  MOCK_STDCALL_METHOD3(GetPrivateData, HRESULT(const GUID&, UINT*, void*));
  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(const GUID&, UINT, const void*));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(const GUID&, const IUnknown*));
  MOCK_STDCALL_METHOD1(GetType, void(D3D11_RESOURCE_DIMENSION*));
  MOCK_STDCALL_METHOD1(SetEvictionPriority, void(UINT));
  MOCK_STDCALL_METHOD0(GetEvictionPriority, UINT());
  MOCK_STDCALL_METHOD1(GetDesc, void(D3D11_TEXTURE2D_DESC*));
};

class D3D11BufferMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D11Buffer> {
 public:
  D3D11BufferMock();
  ~D3D11BufferMock() override;
  MOCK_STDCALL_METHOD1(GetDevice, void(ID3D11Device**));
  MOCK_STDCALL_METHOD3(GetPrivateData, HRESULT(const GUID&, UINT*, void*));
  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(const GUID&, UINT, const void*));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(const GUID&, const IUnknown*));
  MOCK_STDCALL_METHOD1(GetType, void(D3D11_RESOURCE_DIMENSION*));
  MOCK_STDCALL_METHOD1(SetEvictionPriority, void(UINT));
  MOCK_STDCALL_METHOD0(GetEvictionPriority, UINT());
  MOCK_STDCALL_METHOD1(GetDesc, void(D3D11_BUFFER_DESC*));
};

// This classs must mock QueryInterface, since a lot of things are
// QueryInterfac()ed thru this class.
class D3D11DeviceMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D11Device> {
 public:
  D3D11DeviceMock();
  ~D3D11DeviceMock() override;

  MOCK_STDCALL_METHOD2(QueryInterface, HRESULT(REFIID riid, void** ppv));

  MOCK_STDCALL_METHOD3(CreateBuffer,
                       HRESULT(const D3D11_BUFFER_DESC*,
                               const D3D11_SUBRESOURCE_DATA*,
                               ID3D11Buffer**));
  MOCK_STDCALL_METHOD3(CreateTexture1D,
                       HRESULT(const D3D11_TEXTURE1D_DESC*,
                               const D3D11_SUBRESOURCE_DATA*,
                               ID3D11Texture1D**));
  MOCK_STDCALL_METHOD3(CreateTexture2D,
                       HRESULT(const D3D11_TEXTURE2D_DESC*,
                               const D3D11_SUBRESOURCE_DATA*,
                               ID3D11Texture2D**));
  MOCK_STDCALL_METHOD3(CreateTexture3D,
                       HRESULT(const D3D11_TEXTURE3D_DESC*,
                               const D3D11_SUBRESOURCE_DATA*,
                               ID3D11Texture3D**));
  MOCK_STDCALL_METHOD3(CreateShaderResourceView,
                       HRESULT(ID3D11Resource*,
                               const D3D11_SHADER_RESOURCE_VIEW_DESC*,
                               ID3D11ShaderResourceView**));
  MOCK_STDCALL_METHOD3(CreateUnorderedAccessView,
                       HRESULT(ID3D11Resource*,
                               const D3D11_UNORDERED_ACCESS_VIEW_DESC*,
                               ID3D11UnorderedAccessView**));
  MOCK_STDCALL_METHOD3(CreateRenderTargetView,
                       HRESULT(ID3D11Resource*,
                               const D3D11_RENDER_TARGET_VIEW_DESC*,
                               ID3D11RenderTargetView**));
  MOCK_STDCALL_METHOD3(CreateDepthStencilView,
                       HRESULT(ID3D11Resource*,
                               const D3D11_DEPTH_STENCIL_VIEW_DESC*,
                               ID3D11DepthStencilView**));
  MOCK_STDCALL_METHOD5(CreateInputLayout,
                       HRESULT(const D3D11_INPUT_ELEMENT_DESC*,
                               UINT,
                               const void*,
                               SIZE_T,
                               ID3D11InputLayout**));

  MOCK_STDCALL_METHOD4(
      CreateVertexShader,
      HRESULT(const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11VertexShader**));

  MOCK_STDCALL_METHOD4(CreateGeometryShader,
                       HRESULT(const void*,
                               SIZE_T,
                               ID3D11ClassLinkage*,
                               ID3D11GeometryShader**));

  MOCK_STDCALL_METHOD9(CreateGeometryShaderWithStreamOutput,
                       HRESULT(const void*,
                               SIZE_T,
                               const D3D11_SO_DECLARATION_ENTRY*,
                               UINT,
                               const UINT*,
                               UINT,
                               UINT,
                               ID3D11ClassLinkage*,
                               ID3D11GeometryShader**));

  MOCK_STDCALL_METHOD4(
      CreatePixelShader,
      HRESULT(const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11PixelShader**));

  MOCK_STDCALL_METHOD4(
      CreateHullShader,
      HRESULT(const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11HullShader**));

  MOCK_STDCALL_METHOD4(
      CreateDomainShader,
      HRESULT(const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11DomainShader**));

  MOCK_STDCALL_METHOD4(
      CreateComputeShader,
      HRESULT(const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11ComputeShader**));

  MOCK_STDCALL_METHOD1(CreateClassLinkage, HRESULT(ID3D11ClassLinkage**));

  MOCK_STDCALL_METHOD2(CreateBlendState,
                       HRESULT(const D3D11_BLEND_DESC*, ID3D11BlendState**));

  MOCK_STDCALL_METHOD2(CreateDepthStencilState,
                       HRESULT(const D3D11_DEPTH_STENCIL_DESC*,
                               ID3D11DepthStencilState**));

  MOCK_STDCALL_METHOD2(CreateRasterizerState,
                       HRESULT(const D3D11_RASTERIZER_DESC*,
                               ID3D11RasterizerState**));

  MOCK_STDCALL_METHOD2(CreateSamplerState,
                       HRESULT(const D3D11_SAMPLER_DESC*,
                               ID3D11SamplerState**));

  MOCK_STDCALL_METHOD2(CreateQuery,
                       HRESULT(const D3D11_QUERY_DESC*, ID3D11Query**));

  MOCK_STDCALL_METHOD2(CreatePredicate,
                       HRESULT(const D3D11_QUERY_DESC*, ID3D11Predicate**));

  MOCK_STDCALL_METHOD2(CreateCounter,
                       HRESULT(const D3D11_COUNTER_DESC*, ID3D11Counter**));

  MOCK_STDCALL_METHOD2(CreateDeferredContext,
                       HRESULT(UINT, ID3D11DeviceContext**));

  MOCK_STDCALL_METHOD3(OpenSharedResource, HRESULT(HANDLE, const IID&, void**));

  MOCK_STDCALL_METHOD2(CheckFormatSupport, HRESULT(DXGI_FORMAT, UINT*));

  MOCK_STDCALL_METHOD3(CheckMultisampleQualityLevels,
                       HRESULT(DXGI_FORMAT, UINT, UINT*));

  MOCK_STDCALL_METHOD1(CheckCounterInfo, void(D3D11_COUNTER_INFO*));

  MOCK_STDCALL_METHOD9(CheckCounter,
                       HRESULT(const D3D11_COUNTER_DESC*,
                               D3D11_COUNTER_TYPE*,
                               UINT*,
                               LPSTR,
                               UINT*,
                               LPSTR,
                               UINT*,
                               LPSTR,
                               UINT*));

  MOCK_STDCALL_METHOD3(CheckFeatureSupport,
                       HRESULT(D3D11_FEATURE, void*, UINT));

  MOCK_STDCALL_METHOD3(GetPrivateData, HRESULT(const GUID&, UINT*, void*));

  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(const GUID&, UINT, const void*));

  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(const GUID&, const IUnknown*));

  MOCK_STDCALL_METHOD0(GetFeatureLevel, D3D_FEATURE_LEVEL());

  MOCK_STDCALL_METHOD0(GetCreationFlags, UINT());

  MOCK_STDCALL_METHOD0(GetDeviceRemovedReason, HRESULT());

  MOCK_STDCALL_METHOD1(GetImmediateContext, void(ID3D11DeviceContext**));

  MOCK_STDCALL_METHOD1(SetExceptionMode, HRESULT(UINT));

  MOCK_STDCALL_METHOD0(GetExceptionMode, UINT());
};

class DXGIDeviceMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDXGIDevice> {
 public:
  DXGIDeviceMock();
  ~DXGIDeviceMock() override;

  MOCK_STDCALL_METHOD5(CreateSurface,
                       HRESULT(const DXGI_SURFACE_DESC*,
                               UINT,
                               DXGI_USAGE,
                               const DXGI_SHARED_RESOURCE*,
                               IDXGISurface**));
  MOCK_STDCALL_METHOD1(GetAdapter, HRESULT(IDXGIAdapter**));
  MOCK_STDCALL_METHOD1(GetGPUThreadPriority, HRESULT(INT*));
  MOCK_STDCALL_METHOD3(QueryResourceResidency,
                       HRESULT(IUnknown* const*, DXGI_RESIDENCY*, UINT));
  MOCK_STDCALL_METHOD1(SetGPUThreadPriority, HRESULT(INT));
  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(REFGUID, UINT, const void*));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(REFGUID, const IUnknown*));
  MOCK_STDCALL_METHOD2(GetParent, HRESULT(REFIID, void**));
  MOCK_STDCALL_METHOD3(GetPrivateData, HRESULT(REFGUID, UINT*, void*));
};

class DXGIDevice2Mock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDXGIDevice2> {
 public:
  DXGIDevice2Mock();
  ~DXGIDevice2Mock() override;

  MOCK_STDCALL_METHOD1(EnqueueSetEvent, HRESULT(HANDLE));
  MOCK_STDCALL_METHOD3(OfferResources,
                       HRESULT(UINT,
                               IDXGIResource* const*,
                               DXGI_OFFER_RESOURCE_PRIORITY));
  MOCK_STDCALL_METHOD3(ReclaimResources,
                       HRESULT(UINT, IDXGIResource* const*, BOOL*));
  MOCK_STDCALL_METHOD1(GetMaximumFrameLatency, HRESULT(UINT*));
  MOCK_STDCALL_METHOD1(SetMaximumFrameLatency, HRESULT(UINT));

  MOCK_STDCALL_METHOD5(CreateSurface,
                       HRESULT(const DXGI_SURFACE_DESC*,
                               UINT,
                               DXGI_USAGE,
                               const DXGI_SHARED_RESOURCE*,
                               IDXGISurface**));
  MOCK_STDCALL_METHOD1(GetAdapter, HRESULT(IDXGIAdapter**));
  MOCK_STDCALL_METHOD1(GetGPUThreadPriority, HRESULT(INT*));
  MOCK_STDCALL_METHOD3(QueryResourceResidency,
                       HRESULT(IUnknown* const*, DXGI_RESIDENCY*, UINT));
  MOCK_STDCALL_METHOD1(SetGPUThreadPriority, HRESULT(INT));

  MOCK_STDCALL_METHOD2(GetParent, HRESULT(REFIID, void**));
  MOCK_STDCALL_METHOD3(GetPrivateData, HRESULT(REFGUID, UINT*, void*));
  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(REFGUID, UINT, const void*));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(REFGUID, const IUnknown*));
};

class DXGIAdapter3Mock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDXGIAdapter3> {
 public:
  DXGIAdapter3Mock();
  ~DXGIAdapter3Mock() override;

  MOCK_STDCALL_METHOD3(QueryVideoMemoryInfo,
                       HRESULT(UINT,
                               DXGI_MEMORY_SEGMENT_GROUP,
                               DXGI_QUERY_VIDEO_MEMORY_INFO*));
  MOCK_STDCALL_METHOD2(RegisterHardwareContentProtectionTeardownStatusEvent,
                       HRESULT(HANDLE, DWORD*));
  MOCK_STDCALL_METHOD2(RegisterVideoMemoryBudgetChangeNotificationEvent,
                       HRESULT(HANDLE, DWORD*));
  MOCK_STDCALL_METHOD3(SetVideoMemoryReservation,
                       HRESULT(UINT, DXGI_MEMORY_SEGMENT_GROUP, UINT64));
  MOCK_STDCALL_METHOD1(UnregisterHardwareContentProtectionTeardownStatus,
                       void(DWORD));
  MOCK_STDCALL_METHOD1(UnregisterVideoMemoryBudgetChangeNotification,
                       void(DWORD));
  MOCK_STDCALL_METHOD1(GetDesc2, HRESULT(DXGI_ADAPTER_DESC2*));
  MOCK_STDCALL_METHOD1(GetDesc1, HRESULT(DXGI_ADAPTER_DESC1*));
  MOCK_STDCALL_METHOD2(CheckInterfaceSupport, HRESULT(REFGUID, LARGE_INTEGER*));
  MOCK_STDCALL_METHOD2(EnumOutputs, HRESULT(UINT, IDXGIOutput**));
  MOCK_STDCALL_METHOD1(GetDesc, HRESULT(DXGI_ADAPTER_DESC*));
  MOCK_STDCALL_METHOD2(GetParent, HRESULT(REFIID, void**));
  MOCK_STDCALL_METHOD3(GetPrivateData, HRESULT(REFGUID, UINT*, void*));
  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(REFGUID, UINT, const void*));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(REFGUID, const IUnknown*));
};

class DXGIAdapterMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDXGIAdapter> {
 public:
  DXGIAdapterMock();
  ~DXGIAdapterMock() override;

  MOCK_STDCALL_METHOD3(QueryVideoMemoryInfo,
                       HRESULT(UINT,
                               DXGI_MEMORY_SEGMENT_GROUP,
                               DXGI_QUERY_VIDEO_MEMORY_INFO*));
  MOCK_STDCALL_METHOD2(RegisterHardwareContentProtectionTeardownStatusEvent,
                       HRESULT(HANDLE, DWORD*));
  MOCK_STDCALL_METHOD2(RegisterVideoMemoryBudgetChangeNotificationEvent,
                       HRESULT(HANDLE, DWORD*));
  MOCK_STDCALL_METHOD3(SetVideoMemoryReservation,
                       HRESULT(UINT, DXGI_MEMORY_SEGMENT_GROUP, UINT64));
  MOCK_STDCALL_METHOD1(UnregisterHardwareContentProtectionTeardownStatus,
                       void(DWORD));
  MOCK_STDCALL_METHOD1(UnregisterVideoMemoryBudgetChangeNotification,
                       void(DWORD));
  MOCK_STDCALL_METHOD1(GetDesc2, HRESULT(DXGI_ADAPTER_DESC2*));
  MOCK_STDCALL_METHOD1(GetDesc1, HRESULT(DXGI_ADAPTER_DESC1*));
  MOCK_STDCALL_METHOD2(CheckInterfaceSupport, HRESULT(REFGUID, LARGE_INTEGER*));
  MOCK_STDCALL_METHOD2(EnumOutputs, HRESULT(UINT, IDXGIOutput**));
  MOCK_STDCALL_METHOD1(GetDesc, HRESULT(DXGI_ADAPTER_DESC*));
  MOCK_STDCALL_METHOD2(GetParent, HRESULT(REFIID, void**));
  MOCK_STDCALL_METHOD3(GetPrivateData, HRESULT(REFGUID, UINT*, void*));
  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(REFGUID, UINT, const void*));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(REFGUID, const IUnknown*));
};

// TODO(crbug.com/788880): This may not be necessary. Tyr out and see if
// D3D11VideoDevice1Mock is sufficient. and if so, remove this.
class D3D11VideoDeviceMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D11VideoDevice> {
 public:
  D3D11VideoDeviceMock();
  ~D3D11VideoDeviceMock() override;
  MOCK_STDCALL_METHOD3(CreateVideoDecoder,
                       HRESULT(const D3D11_VIDEO_DECODER_DESC*,
                               const D3D11_VIDEO_DECODER_CONFIG*,
                               ID3D11VideoDecoder**));

  MOCK_STDCALL_METHOD3(CreateVideoProcessor,
                       HRESULT(ID3D11VideoProcessorEnumerator*,
                               UINT,
                               ID3D11VideoProcessor**));

  MOCK_STDCALL_METHOD2(CreateAuthenticatedChannel,
                       HRESULT(D3D11_AUTHENTICATED_CHANNEL_TYPE,
                               ID3D11AuthenticatedChannel**));

  MOCK_STDCALL_METHOD4(
      CreateCryptoSession,
      HRESULT(const GUID*, const GUID*, const GUID*, ID3D11CryptoSession**));

  MOCK_STDCALL_METHOD3(CreateVideoDecoderOutputView,
                       HRESULT(ID3D11Resource*,
                               const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC*,
                               ID3D11VideoDecoderOutputView**));

  MOCK_STDCALL_METHOD4(CreateVideoProcessorInputView,
                       HRESULT(ID3D11Resource*,
                               ID3D11VideoProcessorEnumerator*,
                               const D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC*,
                               ID3D11VideoProcessorInputView**));

  MOCK_STDCALL_METHOD4(CreateVideoProcessorOutputView,
                       HRESULT(ID3D11Resource*,
                               ID3D11VideoProcessorEnumerator*,
                               const D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC*,
                               ID3D11VideoProcessorOutputView**));

  MOCK_STDCALL_METHOD2(CreateVideoProcessorEnumerator,
                       HRESULT(const D3D11_VIDEO_PROCESSOR_CONTENT_DESC*,
                               ID3D11VideoProcessorEnumerator**));

  MOCK_STDCALL_METHOD0(GetVideoDecoderProfileCount, UINT());

  MOCK_STDCALL_METHOD2(GetVideoDecoderProfile, HRESULT(UINT, GUID*));

  MOCK_STDCALL_METHOD3(CheckVideoDecoderFormat,
                       HRESULT(const GUID*, DXGI_FORMAT, BOOL*));
  MOCK_STDCALL_METHOD2(GetVideoDecoderConfigCount,
                       HRESULT(const D3D11_VIDEO_DECODER_DESC*, UINT*));

  MOCK_STDCALL_METHOD3(GetVideoDecoderConfig,
                       HRESULT(const D3D11_VIDEO_DECODER_DESC*,
                               UINT,
                               D3D11_VIDEO_DECODER_CONFIG*));

  MOCK_STDCALL_METHOD3(GetContentProtectionCaps,
                       HRESULT(const GUID*,
                               const GUID*,
                               D3D11_VIDEO_CONTENT_PROTECTION_CAPS*));
  MOCK_STDCALL_METHOD4(CheckCryptoKeyExchange,
                       HRESULT(const GUID*, const GUID*, UINT, GUID*));
  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(const GUID&, UINT, const void*));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(const GUID&, const IUnknown*));
};

class D3D11VideoDevice1Mock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D11VideoDevice1> {
 public:
  D3D11VideoDevice1Mock();
  ~D3D11VideoDevice1Mock() override;

  MOCK_STDCALL_METHOD7(CheckVideoDecoderDownsampling,
                       HRESULT(const D3D11_VIDEO_DECODER_DESC* pInputDesc,
                               DXGI_COLOR_SPACE_TYPE InputColorSpace,
                               const D3D11_VIDEO_DECODER_CONFIG* pInputConfig,
                               const DXGI_RATIONAL* pFrameRate,
                               const D3D11_VIDEO_SAMPLE_DESC* pOutputDesc,
                               BOOL* pSupported,
                               BOOL* pRealTimeHint));
  MOCK_STDCALL_METHOD5(GetCryptoSessionPrivateDataSize,
                       HRESULT(const GUID* pCryptoType,
                               const GUID* pDecoderProfile,
                               const GUID* pKeyExchangeType,
                               UINT* pPrivateInputSize,
                               UINT* pPrivateOutputSize));
  MOCK_STDCALL_METHOD7(GetVideoDecoderCaps,
                       HRESULT(const GUID* pDecoderProfile,
                               UINT SampleWidth,
                               UINT SampleHeight,
                               const DXGI_RATIONAL* pFrameRate,
                               UINT BitRate,
                               const GUID* pCryptoType,
                               UINT* pDecoderCaps));
  MOCK_STDCALL_METHOD5(
      RecommendVideoDecoderDownsampleParameters,
      HRESULT(const D3D11_VIDEO_DECODER_DESC* pInputDesc,
              DXGI_COLOR_SPACE_TYPE InputColorSpace,
              const D3D11_VIDEO_DECODER_CONFIG* pInputConfig,
              const DXGI_RATIONAL* pFrameRate,
              D3D11_VIDEO_SAMPLE_DESC* pRecommendedOutputDesc));

  // ID3D11VideoDevice methods.
  MOCK_STDCALL_METHOD3(CreateVideoDecoder,
                       HRESULT(const D3D11_VIDEO_DECODER_DESC*,
                               const D3D11_VIDEO_DECODER_CONFIG*,
                               ID3D11VideoDecoder**));

  MOCK_STDCALL_METHOD3(CreateVideoProcessor,
                       HRESULT(ID3D11VideoProcessorEnumerator*,
                               UINT,
                               ID3D11VideoProcessor**));

  MOCK_STDCALL_METHOD2(CreateAuthenticatedChannel,
                       HRESULT(D3D11_AUTHENTICATED_CHANNEL_TYPE,
                               ID3D11AuthenticatedChannel**));

  MOCK_STDCALL_METHOD4(
      CreateCryptoSession,
      HRESULT(const GUID*, const GUID*, const GUID*, ID3D11CryptoSession**));

  MOCK_STDCALL_METHOD3(CreateVideoDecoderOutputView,
                       HRESULT(ID3D11Resource*,
                               const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC*,
                               ID3D11VideoDecoderOutputView**));

  MOCK_STDCALL_METHOD4(CreateVideoProcessorInputView,
                       HRESULT(ID3D11Resource*,
                               ID3D11VideoProcessorEnumerator*,
                               const D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC*,
                               ID3D11VideoProcessorInputView**));

  MOCK_STDCALL_METHOD4(CreateVideoProcessorOutputView,
                       HRESULT(ID3D11Resource*,
                               ID3D11VideoProcessorEnumerator*,
                               const D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC*,
                               ID3D11VideoProcessorOutputView**));

  MOCK_STDCALL_METHOD2(CreateVideoProcessorEnumerator,
                       HRESULT(const D3D11_VIDEO_PROCESSOR_CONTENT_DESC*,
                               ID3D11VideoProcessorEnumerator**));

  MOCK_STDCALL_METHOD0(GetVideoDecoderProfileCount, UINT());

  MOCK_STDCALL_METHOD2(GetVideoDecoderProfile, HRESULT(UINT, GUID*));

  MOCK_STDCALL_METHOD3(CheckVideoDecoderFormat,
                       HRESULT(const GUID*, DXGI_FORMAT, BOOL*));
  MOCK_STDCALL_METHOD2(GetVideoDecoderConfigCount,
                       HRESULT(const D3D11_VIDEO_DECODER_DESC*, UINT*));

  MOCK_STDCALL_METHOD3(GetVideoDecoderConfig,
                       HRESULT(const D3D11_VIDEO_DECODER_DESC*,
                               UINT,
                               D3D11_VIDEO_DECODER_CONFIG*));

  MOCK_STDCALL_METHOD3(GetContentProtectionCaps,
                       HRESULT(const GUID*,
                               const GUID*,
                               D3D11_VIDEO_CONTENT_PROTECTION_CAPS*));
  MOCK_STDCALL_METHOD4(CheckCryptoKeyExchange,
                       HRESULT(const GUID*, const GUID*, UINT, GUID*));
  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(const GUID&, UINT, const void*));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(const GUID&, const IUnknown*));
};

class D3D11VideoProcessorEnumeratorMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D11VideoProcessorEnumerator> {
 public:
  D3D11VideoProcessorEnumeratorMock();
  ~D3D11VideoProcessorEnumeratorMock() override;

  MOCK_STDCALL_METHOD1(GetDevice, void(ID3D11Device**));
  MOCK_STDCALL_METHOD3(GetPrivateData, HRESULT(const GUID&, UINT*, void*));
  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(const GUID&, UINT, const void*));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(const GUID&, const IUnknown*));

  MOCK_STDCALL_METHOD2(GetVideoProcessorRateConversionCaps,
                       HRESULT(UINT,
                               D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS*));

  MOCK_STDCALL_METHOD2(GetVideoProcessorFilterRange,
                       HRESULT(D3D11_VIDEO_PROCESSOR_FILTER,
                               D3D11_VIDEO_PROCESSOR_FILTER_RANGE*));

  MOCK_STDCALL_METHOD3(GetVideoProcessorCustomRate,
                       HRESULT(UINT, UINT, D3D11_VIDEO_PROCESSOR_CUSTOM_RATE*));

  MOCK_STDCALL_METHOD1(GetVideoProcessorContentDesc,
                       HRESULT(D3D11_VIDEO_PROCESSOR_CONTENT_DESC*));

  MOCK_STDCALL_METHOD1(GetVideoProcessorCaps,
                       HRESULT(D3D11_VIDEO_PROCESSOR_CAPS*));

  MOCK_STDCALL_METHOD2(CheckVideoProcessorFormat, HRESULT(DXGI_FORMAT, UINT*));
};

class D3D11VideoProcessorMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D11VideoProcessor> {
 public:
  D3D11VideoProcessorMock();
  ~D3D11VideoProcessorMock() override;

  MOCK_STDCALL_METHOD1(GetDevice, void(ID3D11Device**));
  MOCK_STDCALL_METHOD3(GetPrivateData, HRESULT(const GUID&, UINT*, void*));
  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(const GUID&, UINT, const void*));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(const GUID&, const IUnknown*));

  MOCK_STDCALL_METHOD1(GetContentDesc,
                       void(D3D11_VIDEO_PROCESSOR_CONTENT_DESC*));

  MOCK_STDCALL_METHOD1(GetRateConversionCaps,
                       void(D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS*));
};

class D3D11VideoContextMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D11VideoContext> {
 public:
  D3D11VideoContextMock();
  ~D3D11VideoContextMock() override;
  MOCK_STDCALL_METHOD1(GetDevice, void(ID3D11Device**));
  MOCK_STDCALL_METHOD3(GetPrivateData, HRESULT(const GUID&, UINT*, void*));
  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(const GUID&, UINT, const void*));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(const GUID&, const IUnknown*));
  MOCK_STDCALL_METHOD4(GetDecoderBuffer,
                       HRESULT(ID3D11VideoDecoder*,
                               D3D11_VIDEO_DECODER_BUFFER_TYPE,
                               UINT*,
                               void**));
  MOCK_STDCALL_METHOD2(ReleaseDecoderBuffer,
                       HRESULT(ID3D11VideoDecoder*,
                               D3D11_VIDEO_DECODER_BUFFER_TYPE));
  MOCK_STDCALL_METHOD4(DecoderBeginFrame,
                       HRESULT(ID3D11VideoDecoder*,
                               ID3D11VideoDecoderOutputView*,
                               UINT,
                               const void*));
  MOCK_STDCALL_METHOD1(DecoderEndFrame, HRESULT(ID3D11VideoDecoder*));
  MOCK_STDCALL_METHOD3(SubmitDecoderBuffers,
                       HRESULT(ID3D11VideoDecoder*,
                               UINT,
                               const D3D11_VIDEO_DECODER_BUFFER_DESC*));
  MOCK_STDCALL_METHOD2(
      DecoderExtension,
      APP_DEPRECATED_HRESULT(ID3D11VideoDecoder*,
                             const D3D11_VIDEO_DECODER_EXTENSION*));
  MOCK_STDCALL_METHOD3(VideoProcessorSetOutputTargetRect,
                       void(ID3D11VideoProcessor*, BOOL, const RECT*));
  MOCK_STDCALL_METHOD3(VideoProcessorSetOutputBackgroundColor,
                       void(ID3D11VideoProcessor*,
                            BOOL,
                            const D3D11_VIDEO_COLOR*));
  MOCK_STDCALL_METHOD2(VideoProcessorSetOutputColorSpace,
                       void(ID3D11VideoProcessor*,
                            const D3D11_VIDEO_PROCESSOR_COLOR_SPACE*));
  MOCK_STDCALL_METHOD3(VideoProcessorSetOutputAlphaFillMode,
                       void(ID3D11VideoProcessor*,
                            D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE,
                            UINT));
  MOCK_STDCALL_METHOD3(VideoProcessorSetOutputConstriction,
                       void(ID3D11VideoProcessor*, BOOL, SIZE));
  MOCK_STDCALL_METHOD2(VideoProcessorSetOutputStereoMode,
                       void(ID3D11VideoProcessor*, BOOL));
  MOCK_STDCALL_METHOD4(
      VideoProcessorSetOutputExtension,
      APP_DEPRECATED_HRESULT(ID3D11VideoProcessor*, const GUID*, UINT, void*));
  MOCK_STDCALL_METHOD3(VideoProcessorGetOutputTargetRect,
                       void(ID3D11VideoProcessor*, BOOL*, RECT*));
  MOCK_STDCALL_METHOD3(VideoProcessorGetOutputBackgroundColor,
                       void(ID3D11VideoProcessor*, BOOL*, D3D11_VIDEO_COLOR*));
  MOCK_STDCALL_METHOD2(VideoProcessorGetOutputColorSpace,
                       void(ID3D11VideoProcessor*,
                            D3D11_VIDEO_PROCESSOR_COLOR_SPACE*));
  MOCK_STDCALL_METHOD3(VideoProcessorGetOutputAlphaFillMode,
                       void(ID3D11VideoProcessor*,
                            D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE*,
                            UINT*));
  MOCK_STDCALL_METHOD3(VideoProcessorGetOutputConstriction,
                       void(ID3D11VideoProcessor*, BOOL*, SIZE*));
  MOCK_STDCALL_METHOD2(VideoProcessorGetOutputStereoMode,
                       void(ID3D11VideoProcessor*, BOOL*));
  MOCK_STDCALL_METHOD4(
      VideoProcessorGetOutputExtension,
      APP_DEPRECATED_HRESULT(ID3D11VideoProcessor*, const GUID*, UINT, void*));
  MOCK_STDCALL_METHOD3(VideoProcessorSetStreamFrameFormat,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            D3D11_VIDEO_FRAME_FORMAT));
  MOCK_STDCALL_METHOD3(VideoProcessorSetStreamColorSpace,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            const D3D11_VIDEO_PROCESSOR_COLOR_SPACE*));
  MOCK_STDCALL_METHOD5(VideoProcessorSetStreamOutputRate,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            D3D11_VIDEO_PROCESSOR_OUTPUT_RATE,
                            BOOL,
                            const DXGI_RATIONAL*));
  MOCK_STDCALL_METHOD4(VideoProcessorSetStreamSourceRect,
                       void(ID3D11VideoProcessor*, UINT, BOOL, const RECT*));
  MOCK_STDCALL_METHOD4(VideoProcessorSetStreamDestRect,
                       void(ID3D11VideoProcessor*, UINT, BOOL, const RECT*));
  MOCK_STDCALL_METHOD4(VideoProcessorSetStreamAlpha,
                       void(ID3D11VideoProcessor*, UINT, BOOL, FLOAT));
  MOCK_STDCALL_METHOD4(VideoProcessorSetStreamPalette,
                       void(ID3D11VideoProcessor*, UINT, UINT, const UINT*));
  MOCK_STDCALL_METHOD5(VideoProcessorSetStreamPixelAspectRatio,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            BOOL,
                            const DXGI_RATIONAL*,
                            const DXGI_RATIONAL*));
  MOCK_STDCALL_METHOD5(VideoProcessorSetStreamLumaKey,
                       void(ID3D11VideoProcessor*, UINT, BOOL, FLOAT, FLOAT));
  MOCK_STDCALL_METHOD8(VideoProcessorSetStreamStereoFormat,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            BOOL,
                            D3D11_VIDEO_PROCESSOR_STEREO_FORMAT,
                            BOOL,
                            BOOL,
                            D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE,
                            int));
  MOCK_STDCALL_METHOD3(VideoProcessorSetStreamAutoProcessingMode,
                       void(ID3D11VideoProcessor*, UINT, BOOL));
  MOCK_STDCALL_METHOD5(VideoProcessorSetStreamFilter,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            D3D11_VIDEO_PROCESSOR_FILTER,
                            BOOL,
                            int));
  MOCK_STDCALL_METHOD5(VideoProcessorSetStreamExtension,
                       APP_DEPRECATED_HRESULT(ID3D11VideoProcessor*,
                                              UINT,
                                              const GUID*,
                                              UINT,
                                              void*));
  MOCK_STDCALL_METHOD3(VideoProcessorGetStreamFrameFormat,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            D3D11_VIDEO_FRAME_FORMAT*));
  MOCK_STDCALL_METHOD3(VideoProcessorGetStreamColorSpace,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            D3D11_VIDEO_PROCESSOR_COLOR_SPACE*));
  MOCK_STDCALL_METHOD5(VideoProcessorGetStreamOutputRate,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            D3D11_VIDEO_PROCESSOR_OUTPUT_RATE*,
                            BOOL*,
                            DXGI_RATIONAL*));
  MOCK_STDCALL_METHOD4(VideoProcessorGetStreamSourceRect,
                       void(ID3D11VideoProcessor*, UINT, BOOL*, RECT*));
  MOCK_STDCALL_METHOD4(VideoProcessorGetStreamDestRect,
                       void(ID3D11VideoProcessor*, UINT, BOOL*, RECT*));
  MOCK_STDCALL_METHOD4(VideoProcessorGetStreamAlpha,
                       void(ID3D11VideoProcessor*, UINT, BOOL*, FLOAT*));
  MOCK_STDCALL_METHOD4(VideoProcessorGetStreamPalette,
                       void(ID3D11VideoProcessor*, UINT, UINT, UINT*));
  MOCK_STDCALL_METHOD5(
      VideoProcessorGetStreamPixelAspectRatio,
      void(ID3D11VideoProcessor*, UINT, BOOL*, DXGI_RATIONAL*, DXGI_RATIONAL*));
  MOCK_STDCALL_METHOD5(
      VideoProcessorGetStreamLumaKey,
      void(ID3D11VideoProcessor*, UINT, BOOL*, FLOAT*, FLOAT*));
  MOCK_STDCALL_METHOD8(VideoProcessorGetStreamStereoFormat,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            BOOL*,
                            D3D11_VIDEO_PROCESSOR_STEREO_FORMAT*,
                            BOOL*,
                            BOOL*,
                            D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE*,
                            int*));
  MOCK_STDCALL_METHOD3(VideoProcessorGetStreamAutoProcessingMode,
                       void(ID3D11VideoProcessor*, UINT, BOOL*));
  MOCK_STDCALL_METHOD5(VideoProcessorGetStreamFilter,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            D3D11_VIDEO_PROCESSOR_FILTER,
                            BOOL*,
                            int*));
  MOCK_STDCALL_METHOD5(VideoProcessorGetStreamExtension,
                       APP_DEPRECATED_HRESULT(ID3D11VideoProcessor*,
                                              UINT,
                                              const GUID*,
                                              UINT,
                                              void*));
  MOCK_STDCALL_METHOD5(VideoProcessorBlt,
                       HRESULT(ID3D11VideoProcessor*,
                               ID3D11VideoProcessorOutputView*,
                               UINT,
                               UINT,
                               const D3D11_VIDEO_PROCESSOR_STREAM*));
  MOCK_STDCALL_METHOD3(NegotiateCryptoSessionKeyExchange,
                       HRESULT(ID3D11CryptoSession*, UINT, void*));
  MOCK_STDCALL_METHOD5(EncryptionBlt,
                       void(ID3D11CryptoSession*,
                            ID3D11Texture2D*,
                            ID3D11Texture2D*,
                            UINT,
                            void*));
  MOCK_STDCALL_METHOD8(DecryptionBlt,
                       void(ID3D11CryptoSession*,
                            ID3D11Texture2D*,
                            ID3D11Texture2D*,
                            D3D11_ENCRYPTED_BLOCK_INFO*,
                            UINT,
                            const void*,
                            UINT,
                            void*));
  MOCK_STDCALL_METHOD3(StartSessionKeyRefresh,
                       void(ID3D11CryptoSession*, UINT, void*));
  MOCK_STDCALL_METHOD1(FinishSessionKeyRefresh, void(ID3D11CryptoSession*));
  MOCK_STDCALL_METHOD3(GetEncryptionBltKey,
                       HRESULT(ID3D11CryptoSession*, UINT, void*));
  MOCK_STDCALL_METHOD3(NegotiateAuthenticatedChannelKeyExchange,
                       HRESULT(ID3D11AuthenticatedChannel*, UINT, void*));
  MOCK_STDCALL_METHOD5(
      QueryAuthenticatedChannel,
      HRESULT(ID3D11AuthenticatedChannel*, UINT, const void*, UINT, void*));
  MOCK_STDCALL_METHOD4(ConfigureAuthenticatedChannel,
                       HRESULT(ID3D11AuthenticatedChannel*,
                               UINT,
                               const void*,
                               D3D11_AUTHENTICATED_CONFIGURE_OUTPUT*));
  MOCK_STDCALL_METHOD4(
      VideoProcessorSetStreamRotation,
      void(ID3D11VideoProcessor*, UINT, BOOL, D3D11_VIDEO_PROCESSOR_ROTATION));
  MOCK_STDCALL_METHOD4(VideoProcessorGetStreamRotation,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            BOOL*,
                            D3D11_VIDEO_PROCESSOR_ROTATION*));
};

class D3D11VideoContext1Mock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D11VideoContext1> {
 public:
  D3D11VideoContext1Mock();
  ~D3D11VideoContext1Mock() override;
  MOCK_STDCALL_METHOD1(GetDevice, void(ID3D11Device**));
  MOCK_STDCALL_METHOD3(GetPrivateData, HRESULT(const GUID&, UINT*, void*));
  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(const GUID&, UINT, const void*));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(const GUID&, const IUnknown*));
  MOCK_STDCALL_METHOD4(GetDecoderBuffer,
                       HRESULT(ID3D11VideoDecoder*,
                               D3D11_VIDEO_DECODER_BUFFER_TYPE,
                               UINT*,
                               void**));
  MOCK_STDCALL_METHOD2(ReleaseDecoderBuffer,
                       HRESULT(ID3D11VideoDecoder*,
                               D3D11_VIDEO_DECODER_BUFFER_TYPE));
  MOCK_STDCALL_METHOD4(DecoderBeginFrame,
                       HRESULT(ID3D11VideoDecoder*,
                               ID3D11VideoDecoderOutputView*,
                               UINT,
                               const void*));
  MOCK_STDCALL_METHOD1(DecoderEndFrame, HRESULT(ID3D11VideoDecoder*));
  MOCK_STDCALL_METHOD3(SubmitDecoderBuffers,
                       HRESULT(ID3D11VideoDecoder*,
                               UINT,
                               const D3D11_VIDEO_DECODER_BUFFER_DESC*));
  MOCK_STDCALL_METHOD2(
      DecoderExtension,
      APP_DEPRECATED_HRESULT(ID3D11VideoDecoder*,
                             const D3D11_VIDEO_DECODER_EXTENSION*));
  MOCK_STDCALL_METHOD3(VideoProcessorSetOutputTargetRect,
                       void(ID3D11VideoProcessor*, BOOL, const RECT*));
  MOCK_STDCALL_METHOD3(VideoProcessorSetOutputBackgroundColor,
                       void(ID3D11VideoProcessor*,
                            BOOL,
                            const D3D11_VIDEO_COLOR*));
  MOCK_STDCALL_METHOD2(VideoProcessorSetOutputColorSpace,
                       void(ID3D11VideoProcessor*,
                            const D3D11_VIDEO_PROCESSOR_COLOR_SPACE*));
  MOCK_STDCALL_METHOD3(VideoProcessorSetOutputAlphaFillMode,
                       void(ID3D11VideoProcessor*,
                            D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE,
                            UINT));
  MOCK_STDCALL_METHOD3(VideoProcessorSetOutputConstriction,
                       void(ID3D11VideoProcessor*, BOOL, SIZE));
  MOCK_STDCALL_METHOD2(VideoProcessorSetOutputStereoMode,
                       void(ID3D11VideoProcessor*, BOOL));
  MOCK_STDCALL_METHOD4(
      VideoProcessorSetOutputExtension,
      APP_DEPRECATED_HRESULT(ID3D11VideoProcessor*, const GUID*, UINT, void*));
  MOCK_STDCALL_METHOD3(VideoProcessorGetOutputTargetRect,
                       void(ID3D11VideoProcessor*, BOOL*, RECT*));
  MOCK_STDCALL_METHOD3(VideoProcessorGetOutputBackgroundColor,
                       void(ID3D11VideoProcessor*, BOOL*, D3D11_VIDEO_COLOR*));
  MOCK_STDCALL_METHOD2(VideoProcessorGetOutputColorSpace,
                       void(ID3D11VideoProcessor*,
                            D3D11_VIDEO_PROCESSOR_COLOR_SPACE*));
  MOCK_STDCALL_METHOD3(VideoProcessorGetOutputAlphaFillMode,
                       void(ID3D11VideoProcessor*,
                            D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE*,
                            UINT*));
  MOCK_STDCALL_METHOD3(VideoProcessorGetOutputConstriction,
                       void(ID3D11VideoProcessor*, BOOL*, SIZE*));
  MOCK_STDCALL_METHOD2(VideoProcessorGetOutputStereoMode,
                       void(ID3D11VideoProcessor*, BOOL*));
  MOCK_STDCALL_METHOD4(
      VideoProcessorGetOutputExtension,
      APP_DEPRECATED_HRESULT(ID3D11VideoProcessor*, const GUID*, UINT, void*));
  MOCK_STDCALL_METHOD3(VideoProcessorSetStreamFrameFormat,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            D3D11_VIDEO_FRAME_FORMAT));
  MOCK_STDCALL_METHOD3(VideoProcessorSetStreamColorSpace,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            const D3D11_VIDEO_PROCESSOR_COLOR_SPACE*));
  MOCK_STDCALL_METHOD5(VideoProcessorSetStreamOutputRate,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            D3D11_VIDEO_PROCESSOR_OUTPUT_RATE,
                            BOOL,
                            const DXGI_RATIONAL*));
  MOCK_STDCALL_METHOD4(VideoProcessorSetStreamSourceRect,
                       void(ID3D11VideoProcessor*, UINT, BOOL, const RECT*));
  MOCK_STDCALL_METHOD4(VideoProcessorSetStreamDestRect,
                       void(ID3D11VideoProcessor*, UINT, BOOL, const RECT*));
  MOCK_STDCALL_METHOD4(VideoProcessorSetStreamAlpha,
                       void(ID3D11VideoProcessor*, UINT, BOOL, FLOAT));
  MOCK_STDCALL_METHOD4(VideoProcessorSetStreamPalette,
                       void(ID3D11VideoProcessor*, UINT, UINT, const UINT*));
  MOCK_STDCALL_METHOD5(VideoProcessorSetStreamPixelAspectRatio,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            BOOL,
                            const DXGI_RATIONAL*,
                            const DXGI_RATIONAL*));
  MOCK_STDCALL_METHOD5(VideoProcessorSetStreamLumaKey,
                       void(ID3D11VideoProcessor*, UINT, BOOL, FLOAT, FLOAT));
  MOCK_STDCALL_METHOD8(VideoProcessorSetStreamStereoFormat,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            BOOL,
                            D3D11_VIDEO_PROCESSOR_STEREO_FORMAT,
                            BOOL,
                            BOOL,
                            D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE,
                            int));
  MOCK_STDCALL_METHOD3(VideoProcessorSetStreamAutoProcessingMode,
                       void(ID3D11VideoProcessor*, UINT, BOOL));
  MOCK_STDCALL_METHOD5(VideoProcessorSetStreamFilter,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            D3D11_VIDEO_PROCESSOR_FILTER,
                            BOOL,
                            int));
  MOCK_STDCALL_METHOD5(VideoProcessorSetStreamExtension,
                       APP_DEPRECATED_HRESULT(ID3D11VideoProcessor*,
                                              UINT,
                                              const GUID*,
                                              UINT,
                                              void*));
  MOCK_STDCALL_METHOD3(VideoProcessorGetStreamFrameFormat,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            D3D11_VIDEO_FRAME_FORMAT*));
  MOCK_STDCALL_METHOD3(VideoProcessorGetStreamColorSpace,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            D3D11_VIDEO_PROCESSOR_COLOR_SPACE*));
  MOCK_STDCALL_METHOD5(VideoProcessorGetStreamOutputRate,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            D3D11_VIDEO_PROCESSOR_OUTPUT_RATE*,
                            BOOL*,
                            DXGI_RATIONAL*));
  MOCK_STDCALL_METHOD4(VideoProcessorGetStreamSourceRect,
                       void(ID3D11VideoProcessor*, UINT, BOOL*, RECT*));
  MOCK_STDCALL_METHOD4(VideoProcessorGetStreamDestRect,
                       void(ID3D11VideoProcessor*, UINT, BOOL*, RECT*));
  MOCK_STDCALL_METHOD4(VideoProcessorGetStreamAlpha,
                       void(ID3D11VideoProcessor*, UINT, BOOL*, FLOAT*));
  MOCK_STDCALL_METHOD4(VideoProcessorGetStreamPalette,
                       void(ID3D11VideoProcessor*, UINT, UINT, UINT*));
  MOCK_STDCALL_METHOD5(
      VideoProcessorGetStreamPixelAspectRatio,
      void(ID3D11VideoProcessor*, UINT, BOOL*, DXGI_RATIONAL*, DXGI_RATIONAL*));
  MOCK_STDCALL_METHOD5(
      VideoProcessorGetStreamLumaKey,
      void(ID3D11VideoProcessor*, UINT, BOOL*, FLOAT*, FLOAT*));
  MOCK_STDCALL_METHOD8(VideoProcessorGetStreamStereoFormat,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            BOOL*,
                            D3D11_VIDEO_PROCESSOR_STEREO_FORMAT*,
                            BOOL*,
                            BOOL*,
                            D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE*,
                            int*));
  MOCK_STDCALL_METHOD3(VideoProcessorGetStreamAutoProcessingMode,
                       void(ID3D11VideoProcessor*, UINT, BOOL*));
  MOCK_STDCALL_METHOD5(VideoProcessorGetStreamFilter,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            D3D11_VIDEO_PROCESSOR_FILTER,
                            BOOL*,
                            int*));
  MOCK_STDCALL_METHOD5(VideoProcessorGetStreamExtension,
                       APP_DEPRECATED_HRESULT(ID3D11VideoProcessor*,
                                              UINT,
                                              const GUID*,
                                              UINT,
                                              void*));
  MOCK_STDCALL_METHOD5(VideoProcessorBlt,
                       HRESULT(ID3D11VideoProcessor*,
                               ID3D11VideoProcessorOutputView*,
                               UINT,
                               UINT,
                               const D3D11_VIDEO_PROCESSOR_STREAM*));
  MOCK_STDCALL_METHOD3(NegotiateCryptoSessionKeyExchange,
                       HRESULT(ID3D11CryptoSession*, UINT, void*));
  MOCK_STDCALL_METHOD5(EncryptionBlt,
                       void(ID3D11CryptoSession*,
                            ID3D11Texture2D*,
                            ID3D11Texture2D*,
                            UINT,
                            void*));
  MOCK_STDCALL_METHOD8(DecryptionBlt,
                       void(ID3D11CryptoSession*,
                            ID3D11Texture2D*,
                            ID3D11Texture2D*,
                            D3D11_ENCRYPTED_BLOCK_INFO*,
                            UINT,
                            const void*,
                            UINT,
                            void*));
  MOCK_STDCALL_METHOD3(StartSessionKeyRefresh,
                       void(ID3D11CryptoSession*, UINT, void*));
  MOCK_STDCALL_METHOD1(FinishSessionKeyRefresh, void(ID3D11CryptoSession*));
  MOCK_STDCALL_METHOD3(GetEncryptionBltKey,
                       HRESULT(ID3D11CryptoSession*, UINT, void*));
  MOCK_STDCALL_METHOD3(NegotiateAuthenticatedChannelKeyExchange,
                       HRESULT(ID3D11AuthenticatedChannel*, UINT, void*));
  MOCK_STDCALL_METHOD5(
      QueryAuthenticatedChannel,
      HRESULT(ID3D11AuthenticatedChannel*, UINT, const void*, UINT, void*));
  MOCK_STDCALL_METHOD4(ConfigureAuthenticatedChannel,
                       HRESULT(ID3D11AuthenticatedChannel*,
                               UINT,
                               const void*,
                               D3D11_AUTHENTICATED_CONFIGURE_OUTPUT*));
  MOCK_STDCALL_METHOD4(
      VideoProcessorSetStreamRotation,
      void(ID3D11VideoProcessor*, UINT, BOOL, D3D11_VIDEO_PROCESSOR_ROTATION));
  MOCK_STDCALL_METHOD4(VideoProcessorGetStreamRotation,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            BOOL*,
                            D3D11_VIDEO_PROCESSOR_ROTATION*));

  MOCK_STDCALL_METHOD3(SubmitDecoderBuffers1,
                       HRESULT(ID3D11VideoDecoder*,
                               UINT,
                               const D3D11_VIDEO_DECODER_BUFFER_DESC1*));
  MOCK_STDCALL_METHOD4(
      GetDataForNewHardwareKey,
      HRESULT(ID3D11CryptoSession*, UINT, const void*, UINT64*));
  MOCK_STDCALL_METHOD2(CheckCryptoSessionStatus,
                       HRESULT(ID3D11CryptoSession*,
                               D3D11_CRYPTO_SESSION_STATUS*));
  MOCK_STDCALL_METHOD4(DecoderEnableDownsampling,
                       HRESULT(ID3D11VideoDecoder*,
                               DXGI_COLOR_SPACE_TYPE,
                               const D3D11_VIDEO_SAMPLE_DESC*,
                               UINT));
  MOCK_STDCALL_METHOD2(DecoderUpdateDownsampling,
                       HRESULT(ID3D11VideoDecoder*,
                               const D3D11_VIDEO_SAMPLE_DESC*));
  MOCK_STDCALL_METHOD2(VideoProcessorSetOutputColorSpace1,
                       void(ID3D11VideoProcessor*, DXGI_COLOR_SPACE_TYPE));
  MOCK_STDCALL_METHOD2(VideoProcessorSetOutputShaderUsage,
                       void(ID3D11VideoProcessor*, BOOL));
  MOCK_STDCALL_METHOD2(VideoProcessorGetOutputColorSpace1,
                       void(ID3D11VideoProcessor*, DXGI_COLOR_SPACE_TYPE*));
  MOCK_STDCALL_METHOD2(VideoProcessorGetOutputShaderUsage,
                       void(ID3D11VideoProcessor*, BOOL*));
  MOCK_STDCALL_METHOD3(VideoProcessorSetStreamColorSpace1,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            DXGI_COLOR_SPACE_TYPE));
  MOCK_STDCALL_METHOD5(VideoProcessorSetStreamMirror,
                       void(ID3D11VideoProcessor*, UINT, BOOL, BOOL, BOOL));
  MOCK_STDCALL_METHOD3(VideoProcessorGetStreamColorSpace1,
                       void(ID3D11VideoProcessor*,
                            UINT,
                            DXGI_COLOR_SPACE_TYPE*));
  MOCK_STDCALL_METHOD5(VideoProcessorGetStreamMirror,
                       void(ID3D11VideoProcessor*, UINT, BOOL*, BOOL*, BOOL*));
  MOCK_STDCALL_METHOD7(
      VideoProcessorGetBehaviorHints,
      HRESULT(ID3D11VideoProcessor*,
              UINT,
              UINT,
              DXGI_FORMAT,
              UINT,
              const D3D11_VIDEO_PROCESSOR_STREAM_BEHAVIOR_HINT*,
              UINT*));
};

class D3D11VideoDecoderMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D11VideoDecoder> {
 public:
  D3D11VideoDecoderMock();
  ~D3D11VideoDecoderMock() override;
  MOCK_STDCALL_METHOD1(GetDevice, void(ID3D11Device**));
  MOCK_STDCALL_METHOD3(GetPrivateData, HRESULT(const GUID&, UINT*, void*));
  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(const GUID&, UINT, const void*));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(const GUID&, const IUnknown*));
  MOCK_STDCALL_METHOD2(GetCreationParameters,
                       HRESULT(D3D11_VIDEO_DECODER_DESC*,
                               D3D11_VIDEO_DECODER_CONFIG*));
  MOCK_STDCALL_METHOD1(GetDriverHandle, HRESULT(HANDLE*));
};

class D3D11CryptoSessionMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D11CryptoSession> {
 public:
  D3D11CryptoSessionMock();
  ~D3D11CryptoSessionMock() override;
  MOCK_STDCALL_METHOD1(GetDevice, void(ID3D11Device**));
  MOCK_STDCALL_METHOD3(GetPrivateData, HRESULT(const GUID&, UINT*, void*));
  MOCK_STDCALL_METHOD3(SetPrivateData, HRESULT(const GUID&, UINT, const void*));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(const GUID&, const IUnknown*));
  MOCK_STDCALL_METHOD1(GetCryptoType, void(GUID*));
  MOCK_STDCALL_METHOD1(GetDecoderProfile, void(GUID*));
  MOCK_STDCALL_METHOD1(GetCertificateSize, HRESULT(UINT*));
  MOCK_STDCALL_METHOD2(GetCertificate, HRESULT(UINT, BYTE*));
  MOCK_STDCALL_METHOD1(GetCryptoSessionHandle, void(HANDLE*));
};

// This classs must mock QueryInterface, since a lot of things are
// QueryInterfac()ed thru this class.
class D3D11DeviceContextMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D11DeviceContext> {
 public:
  D3D11DeviceContextMock();
  ~D3D11DeviceContextMock() override;

  MOCK_STDCALL_METHOD2(QueryInterface, HRESULT(REFIID riid, void** ppv));

  // ID3D11DevieChild methods.
  MOCK_STDCALL_METHOD1(GetDevice, void(ID3D11Device** ppDevice));
  MOCK_STDCALL_METHOD3(GetPrivateData,
                       HRESULT(REFGUID guid, UINT* pDataSize, void* pData));
  MOCK_STDCALL_METHOD3(SetPrivateData,
                       HRESULT(REFGUID guid, UINT DataSize, const void* pData));
  MOCK_STDCALL_METHOD2(SetPrivateDataInterface,
                       HRESULT(REFGUID guid, const IUnknown* pData));

  // ID3D11DeviceContext methods.
  MOCK_STDCALL_METHOD3(VSSetConstantBuffers,
                       void(UINT StartSlot,
                            UINT NumBuffers,
                            ID3D11Buffer* const* ppConstantBuffers));

  MOCK_STDCALL_METHOD3(
      PSSetShaderResources,
      void(UINT StartSlot,
           UINT NumViews,
           ID3D11ShaderResourceView* const* ppShaderResourceViews));

  MOCK_STDCALL_METHOD3(PSSetShader,
                       void(ID3D11PixelShader* pPixelShader,
                            ID3D11ClassInstance* const* ppClassInstances,
                            UINT NumClassInstances));

  MOCK_STDCALL_METHOD3(PSSetSamplers,
                       void(UINT StartSlot,
                            UINT NumSamplers,
                            ID3D11SamplerState* const* ppSamplers));

  MOCK_STDCALL_METHOD3(VSSetShader,
                       void(ID3D11VertexShader* pVertexShader,
                            ID3D11ClassInstance* const* ppClassInstances,
                            UINT NumClassInstances));

  MOCK_STDCALL_METHOD3(DrawIndexed,
                       void(UINT IndexCount,
                            UINT StartIndexLocation,
                            INT BaseVertexLocation));

  MOCK_STDCALL_METHOD2(Draw, void(UINT VertexCount, UINT StartVertexLocation));

  MOCK_STDCALL_METHOD5(Map,
                       HRESULT(ID3D11Resource* pResource,
                               UINT Subresource,
                               D3D11_MAP MapType,
                               UINT MapFlags,
                               D3D11_MAPPED_SUBRESOURCE* pMappedResource));

  MOCK_STDCALL_METHOD2(Unmap,
                       void(ID3D11Resource* pResource, UINT Subresource));

  MOCK_STDCALL_METHOD3(PSSetConstantBuffers,
                       void(UINT StartSlot,
                            UINT NumBuffers,
                            ID3D11Buffer* const* ppConstantBuffers));

  MOCK_STDCALL_METHOD1(IASetInputLayout, void(ID3D11InputLayout* pInputLayout));

  MOCK_STDCALL_METHOD5(IASetVertexBuffers,
                       void(UINT StartSlot,
                            UINT NumBuffers,
                            ID3D11Buffer* const* ppVertexBuffers,
                            const UINT* pStrides,
                            const UINT* pOffsets));

  MOCK_STDCALL_METHOD3(IASetIndexBuffer,
                       void(ID3D11Buffer* pIndexBuffer,
                            DXGI_FORMAT Format,
                            UINT Offset));

  MOCK_STDCALL_METHOD5(DrawIndexedInstanced,
                       void(UINT IndexCountPerInstance,
                            UINT InstanceCount,
                            UINT StartIndexLocation,
                            INT BaseVertexLocation,
                            UINT StartInstanceLocation));

  MOCK_STDCALL_METHOD4(DrawInstanced,
                       void(UINT VertexCountPerInstance,
                            UINT InstanceCount,
                            UINT StartVertexLocation,
                            UINT StartInstanceLocation));

  MOCK_STDCALL_METHOD3(GSSetConstantBuffers,
                       void(UINT StartSlot,
                            UINT NumBuffers,
                            ID3D11Buffer* const* ppConstantBuffers));

  MOCK_STDCALL_METHOD3(GSSetShader,
                       void(ID3D11GeometryShader* pShader,
                            ID3D11ClassInstance* const* ppClassInstances,
                            UINT NumClassInstances));

  MOCK_STDCALL_METHOD1(IASetPrimitiveTopology,
                       void(D3D11_PRIMITIVE_TOPOLOGY Topology));

  MOCK_STDCALL_METHOD3(
      VSSetShaderResources,
      void(UINT StartSlot,
           UINT NumViews,
           ID3D11ShaderResourceView* const* ppShaderResourceViews));

  MOCK_STDCALL_METHOD3(VSSetSamplers,
                       void(UINT StartSlot,
                            UINT NumSamplers,
                            ID3D11SamplerState* const* ppSamplers));

  MOCK_STDCALL_METHOD1(Begin, void(ID3D11Asynchronous* pAsync));

  MOCK_STDCALL_METHOD1(End, void(ID3D11Asynchronous* pAsync));

  MOCK_STDCALL_METHOD4(GetData,
                       HRESULT(ID3D11Asynchronous* pAsync,
                               void* pData,
                               UINT DataSize,
                               UINT GetDataFlags));

  MOCK_STDCALL_METHOD2(SetPredication,
                       void(ID3D11Predicate* pPredicate, BOOL PredicateValue));

  MOCK_STDCALL_METHOD3(
      GSSetShaderResources,
      void(UINT StartSlot,
           UINT NumViews,
           ID3D11ShaderResourceView* const* ppShaderResourceViews));

  MOCK_STDCALL_METHOD3(GSSetSamplers,
                       void(UINT StartSlot,
                            UINT NumSamplers,
                            ID3D11SamplerState* const* ppSamplers));

  MOCK_STDCALL_METHOD3(OMSetRenderTargets,
                       void(UINT NumViews,
                            ID3D11RenderTargetView* const* ppRenderTargetViews,
                            ID3D11DepthStencilView* pDepthStencilView));

  MOCK_STDCALL_METHOD7(
      OMSetRenderTargetsAndUnorderedAccessViews,
      void(UINT NumRTVs,
           ID3D11RenderTargetView* const* ppRenderTargetViews,
           ID3D11DepthStencilView* pDepthStencilView,
           UINT UAVStartSlot,
           UINT NumUAVs,
           ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
           const UINT* pUAVInitialCounts));

  MOCK_STDCALL_METHOD3(OMSetBlendState,
                       void(ID3D11BlendState* pBlendState,
                            const FLOAT BlendFactor[4],
                            UINT SampleMask));

  MOCK_STDCALL_METHOD2(OMSetDepthStencilState,
                       void(ID3D11DepthStencilState* pDepthStencilState,
                            UINT StencilRef));

  MOCK_STDCALL_METHOD3(SOSetTargets,
                       void(UINT NumBuffers,
                            ID3D11Buffer* const* ppSOTargets,
                            const UINT* pOffsets));

  MOCK_STDCALL_METHOD0(DrawAuto, void());

  MOCK_STDCALL_METHOD2(DrawIndexedInstancedIndirect,
                       void(ID3D11Buffer* pBufferForArgs,
                            UINT AlignedByteOffsetForArgs));

  MOCK_STDCALL_METHOD2(DrawInstancedIndirect,
                       void(ID3D11Buffer* pBufferForArgs,
                            UINT AlignedByteOffsetForArgs));

  MOCK_STDCALL_METHOD3(Dispatch,
                       void(UINT ThreadGroupCountX,
                            UINT ThreadGroupCountY,
                            UINT ThreadGroupCountZ));

  MOCK_STDCALL_METHOD2(DispatchIndirect,
                       void(ID3D11Buffer* pBufferForArgs,
                            UINT AlignedByteOffsetForArgs));

  MOCK_STDCALL_METHOD1(RSSetState,
                       void(ID3D11RasterizerState* pRasterizerState));

  MOCK_STDCALL_METHOD2(RSSetViewports,
                       void(UINT NumViewports,
                            const D3D11_VIEWPORT* pViewports));

  MOCK_STDCALL_METHOD2(RSSetScissorRects,
                       void(UINT NumRects, const D3D11_RECT* pRects));

  MOCK_STDCALL_METHOD8(CopySubresourceRegion,
                       void(ID3D11Resource* pDstResource,
                            UINT DstSubresource,
                            UINT DstX,
                            UINT DstY,
                            UINT DstZ,
                            ID3D11Resource* pSrcResource,
                            UINT SrcSubresource,
                            const D3D11_BOX* pSrcBox));

  MOCK_STDCALL_METHOD2(CopyResource,
                       void(ID3D11Resource* pDstResource,
                            ID3D11Resource* pSrcResource));

  MOCK_STDCALL_METHOD6(UpdateSubresource,
                       void(ID3D11Resource* pDstResource,
                            UINT DstSubresource,
                            const D3D11_BOX* pDstBox,
                            const void* pSrcData,
                            UINT SrcRowPitch,
                            UINT SrcDepthPitch));

  MOCK_STDCALL_METHOD3(CopyStructureCount,
                       void(ID3D11Buffer* pDstBuffer,
                            UINT DstAlignedByteOffset,
                            ID3D11UnorderedAccessView* pSrcView));

  MOCK_STDCALL_METHOD2(ClearRenderTargetView,
                       void(ID3D11RenderTargetView* pRenderTargetView,
                            const FLOAT ColorRGBA[4]));

  MOCK_STDCALL_METHOD2(ClearUnorderedAccessViewUint,
                       void(ID3D11UnorderedAccessView* pUnorderedAccessView,
                            const UINT Values[4]));

  MOCK_STDCALL_METHOD2(ClearUnorderedAccessViewFloat,
                       void(ID3D11UnorderedAccessView* pUnorderedAccessView,
                            const FLOAT Values[4]));

  MOCK_STDCALL_METHOD4(ClearDepthStencilView,
                       void(ID3D11DepthStencilView* pDepthStencilView,
                            UINT ClearFlags,
                            FLOAT Depth,
                            UINT8 Stencil));

  MOCK_STDCALL_METHOD1(GenerateMips,
                       void(ID3D11ShaderResourceView* pShaderResourceView));

  MOCK_STDCALL_METHOD2(SetResourceMinLOD,
                       void(ID3D11Resource* pResource, FLOAT MinLOD));

  MOCK_STDCALL_METHOD1(GetResourceMinLOD, FLOAT(ID3D11Resource* pResource));

  MOCK_STDCALL_METHOD5(ResolveSubresource,
                       void(ID3D11Resource* pDstResource,
                            UINT DstSubresource,
                            ID3D11Resource* pSrcResource,
                            UINT SrcSubresource,
                            DXGI_FORMAT Format));

  MOCK_STDCALL_METHOD2(ExecuteCommandList,
                       void(ID3D11CommandList* pCommandList,
                            BOOL RestoreContextState));

  MOCK_STDCALL_METHOD3(
      HSSetShaderResources,
      void(UINT StartSlot,
           UINT NumViews,
           ID3D11ShaderResourceView* const* ppShaderResourceViews));

  MOCK_STDCALL_METHOD3(HSSetShader,
                       void(ID3D11HullShader* pHullShader,
                            ID3D11ClassInstance* const* ppClassInstances,
                            UINT NumClassInstances));

  MOCK_STDCALL_METHOD3(HSSetSamplers,
                       void(UINT StartSlot,
                            UINT NumSamplers,
                            ID3D11SamplerState* const* ppSamplers));

  MOCK_STDCALL_METHOD3(HSSetConstantBuffers,
                       void(UINT StartSlot,
                            UINT NumBuffers,
                            ID3D11Buffer* const* ppConstantBuffers));

  MOCK_STDCALL_METHOD3(
      DSSetShaderResources,
      void(UINT StartSlot,
           UINT NumViews,
           ID3D11ShaderResourceView* const* ppShaderResourceViews));

  MOCK_STDCALL_METHOD3(DSSetShader,
                       void(ID3D11DomainShader* pDomainShader,
                            ID3D11ClassInstance* const* ppClassInstances,
                            UINT NumClassInstances));

  MOCK_STDCALL_METHOD3(DSSetSamplers,
                       void(UINT StartSlot,
                            UINT NumSamplers,
                            ID3D11SamplerState* const* ppSamplers));

  MOCK_STDCALL_METHOD3(DSSetConstantBuffers,
                       void(UINT StartSlot,
                            UINT NumBuffers,
                            ID3D11Buffer* const* ppConstantBuffers));

  MOCK_STDCALL_METHOD3(
      CSSetShaderResources,
      void(UINT StartSlot,
           UINT NumViews,
           ID3D11ShaderResourceView* const* ppShaderResourceViews));

  MOCK_STDCALL_METHOD4(
      CSSetUnorderedAccessViews,
      void(UINT StartSlot,
           UINT NumUAVs,
           ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
           const UINT* pUAVInitialCounts));

  MOCK_STDCALL_METHOD3(CSSetShader,
                       void(ID3D11ComputeShader* pComputeShader,
                            ID3D11ClassInstance* const* ppClassInstances,
                            UINT NumClassInstances));

  MOCK_STDCALL_METHOD3(CSSetSamplers,
                       void(UINT StartSlot,
                            UINT NumSamplers,
                            ID3D11SamplerState* const* ppSamplers));

  MOCK_STDCALL_METHOD3(CSSetConstantBuffers,
                       void(UINT StartSlot,
                            UINT NumBuffers,
                            ID3D11Buffer* const* ppConstantBuffers));

  MOCK_STDCALL_METHOD3(VSGetConstantBuffers,
                       void(UINT StartSlot,
                            UINT NumBuffers,
                            ID3D11Buffer** ppConstantBuffers));

  MOCK_STDCALL_METHOD3(PSGetShaderResources,
                       void(UINT StartSlot,
                            UINT NumViews,
                            ID3D11ShaderResourceView** ppShaderResourceViews));

  MOCK_STDCALL_METHOD3(PSGetShader,
                       void(ID3D11PixelShader** ppPixelShader,
                            ID3D11ClassInstance** ppClassInstances,
                            UINT* pNumClassInstances));

  MOCK_STDCALL_METHOD3(PSGetSamplers,
                       void(UINT StartSlot,
                            UINT NumSamplers,
                            ID3D11SamplerState** ppSamplers));

  MOCK_STDCALL_METHOD3(VSGetShader,
                       void(ID3D11VertexShader** ppVertexShader,
                            ID3D11ClassInstance** ppClassInstances,
                            UINT* pNumClassInstances));

  MOCK_STDCALL_METHOD3(PSGetConstantBuffers,
                       void(UINT StartSlot,
                            UINT NumBuffers,
                            ID3D11Buffer** ppConstantBuffers));

  MOCK_STDCALL_METHOD1(IAGetInputLayout,
                       void(ID3D11InputLayout** ppInputLayout));

  MOCK_STDCALL_METHOD5(IAGetVertexBuffers,
                       void(UINT StartSlot,
                            UINT NumBuffers,
                            ID3D11Buffer** ppVertexBuffers,
                            UINT* pStrides,
                            UINT* pOffsets));

  MOCK_STDCALL_METHOD3(IAGetIndexBuffer,
                       void(ID3D11Buffer** pIndexBuffer,
                            DXGI_FORMAT* Format,
                            UINT* Offset));

  MOCK_STDCALL_METHOD3(GSGetConstantBuffers,
                       void(UINT StartSlot,
                            UINT NumBuffers,
                            ID3D11Buffer** ppConstantBuffers));

  MOCK_STDCALL_METHOD3(GSGetShader,
                       void(ID3D11GeometryShader** ppGeometryShader,
                            ID3D11ClassInstance** ppClassInstances,
                            UINT* pNumClassInstances));

  MOCK_STDCALL_METHOD1(IAGetPrimitiveTopology,
                       void(D3D11_PRIMITIVE_TOPOLOGY* pTopology));

  MOCK_STDCALL_METHOD3(VSGetShaderResources,
                       void(UINT StartSlot,
                            UINT NumViews,
                            ID3D11ShaderResourceView** ppShaderResourceViews));

  MOCK_STDCALL_METHOD3(VSGetSamplers,
                       void(UINT StartSlot,
                            UINT NumSamplers,
                            ID3D11SamplerState** ppSamplers));

  MOCK_STDCALL_METHOD2(GetPredication,
                       void(ID3D11Predicate** ppPredicate,
                            BOOL* pPredicateValue));

  MOCK_STDCALL_METHOD3(GSGetShaderResources,
                       void(UINT StartSlot,
                            UINT NumViews,
                            ID3D11ShaderResourceView** ppShaderResourceViews));

  MOCK_STDCALL_METHOD3(GSGetSamplers,
                       void(UINT StartSlot,
                            UINT NumSamplers,
                            ID3D11SamplerState** ppSamplers));

  MOCK_STDCALL_METHOD3(OMGetRenderTargets,
                       void(UINT NumViews,
                            ID3D11RenderTargetView** ppRenderTargetViews,
                            ID3D11DepthStencilView** ppDepthStencilView));

  MOCK_STDCALL_METHOD6(
      OMGetRenderTargetsAndUnorderedAccessViews,
      void(UINT NumRTVs,
           ID3D11RenderTargetView** ppRenderTargetViews,
           ID3D11DepthStencilView** ppDepthStencilView,
           UINT UAVStartSlot,
           UINT NumUAVs,
           ID3D11UnorderedAccessView** ppUnorderedAccessViews));

  MOCK_STDCALL_METHOD3(OMGetBlendState,
                       void(ID3D11BlendState** ppBlendState,
                            FLOAT BlendFactor[4],
                            UINT* pSampleMask));

  MOCK_STDCALL_METHOD2(OMGetDepthStencilState,
                       void(ID3D11DepthStencilState** ppDepthStencilState,
                            UINT* pStencilRef));

  MOCK_STDCALL_METHOD2(SOGetTargets,
                       void(UINT NumBuffers, ID3D11Buffer** ppSOTargets));

  MOCK_STDCALL_METHOD1(RSGetState,
                       void(ID3D11RasterizerState** ppRasterizerState));

  MOCK_STDCALL_METHOD2(RSGetViewports,
                       void(UINT* pNumViewports, D3D11_VIEWPORT* pViewports));

  MOCK_STDCALL_METHOD2(RSGetScissorRects,
                       void(UINT* pNumRects, D3D11_RECT* pRects));

  MOCK_STDCALL_METHOD3(HSGetShaderResources,
                       void(UINT StartSlot,
                            UINT NumViews,
                            ID3D11ShaderResourceView** ppShaderResourceViews));

  MOCK_STDCALL_METHOD3(HSGetShader,
                       void(ID3D11HullShader** ppHullShader,
                            ID3D11ClassInstance** ppClassInstances,
                            UINT* pNumClassInstances));

  MOCK_STDCALL_METHOD3(HSGetSamplers,
                       void(UINT StartSlot,
                            UINT NumSamplers,
                            ID3D11SamplerState** ppSamplers));

  MOCK_STDCALL_METHOD3(HSGetConstantBuffers,
                       void(UINT StartSlot,
                            UINT NumBuffers,
                            ID3D11Buffer** ppConstantBuffers));

  MOCK_STDCALL_METHOD3(DSGetShaderResources,
                       void(UINT StartSlot,
                            UINT NumViews,
                            ID3D11ShaderResourceView** ppShaderResourceViews));

  MOCK_STDCALL_METHOD3(DSGetShader,
                       void(ID3D11DomainShader** ppDomainShader,
                            ID3D11ClassInstance** ppClassInstances,
                            UINT* pNumClassInstances));

  MOCK_STDCALL_METHOD3(DSGetSamplers,
                       void(UINT StartSlot,
                            UINT NumSamplers,
                            ID3D11SamplerState** ppSamplers));

  MOCK_STDCALL_METHOD3(DSGetConstantBuffers,
                       void(UINT StartSlot,
                            UINT NumBuffers,
                            ID3D11Buffer** ppConstantBuffers));

  MOCK_STDCALL_METHOD3(CSGetShaderResources,
                       void(UINT StartSlot,
                            UINT NumViews,
                            ID3D11ShaderResourceView** ppShaderResourceViews));

  MOCK_STDCALL_METHOD3(
      CSGetUnorderedAccessViews,
      void(UINT StartSlot,
           UINT NumUAVs,
           ID3D11UnorderedAccessView** ppUnorderedAccessViews));

  MOCK_STDCALL_METHOD3(CSGetShader,
                       void(ID3D11ComputeShader** ppComputeShader,
                            ID3D11ClassInstance** ppClassInstances,
                            UINT* pNumClassInstances));

  MOCK_STDCALL_METHOD3(CSGetSamplers,
                       void(UINT StartSlot,
                            UINT NumSamplers,
                            ID3D11SamplerState** ppSamplers));

  MOCK_STDCALL_METHOD3(CSGetConstantBuffers,
                       void(UINT StartSlot,
                            UINT NumBuffers,
                            ID3D11Buffer** ppConstantBuffers));

  MOCK_STDCALL_METHOD0(ClearState, void());

  MOCK_STDCALL_METHOD0(Flush, void());

  MOCK_STDCALL_METHOD0(GetType, D3D11_DEVICE_CONTEXT_TYPE());

  MOCK_STDCALL_METHOD0(GetContextFlags, UINT());

  MOCK_STDCALL_METHOD2(FinishCommandList,
                       HRESULT(BOOL RestoreDeferredContextState,
                               ID3D11CommandList** ppCommandList));
};
}  // namespace media

#undef MOCK_STDCALL_METHOD0
#undef MOCK_STDCALL_METHOD1
#undef MOCK_STDCALL_METHOD2
#undef MOCK_STDCALL_METHOD3
#undef MOCK_STDCALL_METHOD4
#undef MOCK_STDCALL_METHOD5
#undef MOCK_STDCALL_METHOD6
#undef MOCK_STDCALL_METHOD7
#undef MOCK_STDCALL_METHOD8
#undef MOCK_STDCALL_METHOD9

#endif  // MEDIA_BASE_WIN_D3D11_MOCKS_H_
