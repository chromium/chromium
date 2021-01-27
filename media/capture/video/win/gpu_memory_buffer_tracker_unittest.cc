// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mfidl.h>

#include <dxgi1_2.h>
#include <mfapi.h>
#include <mferror.h>
#include <wrl.h>
#include <wrl/client.h>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/win/windows_version.h"
#include "media/capture/video/win/gpu_memory_buffer_tracker.h"
#include "media/capture/video/win/video_capture_device_factory_win.h"
#include "media/capture/video/win/video_capture_dxgi_device_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Pointee;

namespace media {

namespace {

template <class Interface>
class MockInterface
    : public base::RefCountedThreadSafe<MockInterface<Interface>>,
      public Interface {
 public:
  // IUnknown
  IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (riid == __uuidof(this) || riid == __uuidof(IUnknown)) {
      this->AddRef();
      *object = this;
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  IFACEMETHODIMP_(ULONG) AddRef() override {
    base::RefCountedThreadSafe<MockInterface>::AddRef();
    return 1U;
  }
  IFACEMETHODIMP_(ULONG) Release() override {
    base::RefCountedThreadSafe<MockInterface>::Release();
    return 1U;
  }

 protected:
  friend class base::RefCountedThreadSafe<MockInterface<Interface>>;
  virtual ~MockInterface() = default;
};

class MockDXGIResource final : public MockInterface<IDXGIResource1> {
 public:
  // IDXGIResource1
  IFACEMETHODIMP CreateSubresourceSurface(UINT index, IDXGISurface2** surface) {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP CreateSharedHandle(const SECURITY_ATTRIBUTES* attributes,
                                    DWORD access,
                                    LPCWSTR name,
                                    HANDLE* handle) {
    // Need to provide a real handle to client, so create an event handle
    *handle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    return S_OK;
  }
  // IDXGIResource
  IFACEMETHODIMP GetSharedHandle(HANDLE* shared_handle) { return E_NOTIMPL; }
  IFACEMETHODIMP GetUsage(DXGI_USAGE* usage) { return E_NOTIMPL; }
  IFACEMETHODIMP SetEvictionPriority(UINT eviction_priority) {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP GetEvictionPriority(UINT* eviction_priority) {
    return E_NOTIMPL;
  }
  // IDXGIDeviceSubObject
  IFACEMETHODIMP GetDevice(REFIID riid, void** device) { return E_NOTIMPL; }
  // IDXGIObject
  IFACEMETHODIMP SetPrivateData(REFGUID name,
                                UINT data_size,
                                const void* data) {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP SetPrivateDataInterface(REFGUID name,
                                         const IUnknown* unknown) {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP GetPrivateData(REFGUID name, UINT* data_size, void* data) {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP GetParent(REFIID riid, void** parent) { return E_NOTIMPL; }

 private:
  ~MockDXGIResource() override = default;
};

class MockD3D11Texture2D final : public MockInterface<ID3D11Texture2D> {
 public:
  // IUnknown
  IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (riid == __uuidof(IDXGIResource1)) {
      Microsoft::WRL::ComPtr<MockDXGIResource> mock_resource(
          new MockDXGIResource());
      mock_resource->AddRef();
      return mock_resource.CopyTo(riid, object);
    }
    return MockInterface::QueryInterface(riid, object);
  }

  // ID3D11Texture2D
  IFACEMETHODIMP_(void) GetDesc(D3D11_TEXTURE2D_DESC* desc) {}
  // ID3D11Resource
  IFACEMETHODIMP_(void) GetType(D3D11_RESOURCE_DIMENSION* resource_dimension) {}
  IFACEMETHODIMP_(void) SetEvictionPriority(UINT eviction_priority) {}
  IFACEMETHODIMP_(UINT) GetEvictionPriority() { return 0; }
  // ID3D11DeviceChild
  IFACEMETHODIMP_(void) GetDevice(ID3D11Device** device) {}
  IFACEMETHODIMP GetPrivateData(REFGUID guid, UINT* data_size, void* data) {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP SetPrivateData(REFGUID guid,
                                UINT data_size,
                                const void* data) {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP SetPrivateDataInterface(REFGUID guid, const IUnknown* data) {
    return E_NOTIMPL;
  }

 private:
  ~MockD3D11Texture2D() override = default;
};

class MockD3D11Device final : public MockInterface<ID3D11Device> {
 public:
  // ID3D11Device
  IFACEMETHODIMP CreateBuffer(const D3D11_BUFFER_DESC* desc,
                              const D3D11_SUBRESOURCE_DATA* initial_data,
                              ID3D11Buffer** ppBuffer) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateTexture1D(const D3D11_TEXTURE1D_DESC* desc,
                                 const D3D11_SUBRESOURCE_DATA* initial_data,
                                 ID3D11Texture1D** texture1D) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateTexture2D(const D3D11_TEXTURE2D_DESC* desc,
                                 const D3D11_SUBRESOURCE_DATA* initial_data,
                                 ID3D11Texture2D** texture2D) {
    OnCreateTexture2D(desc, initial_data, texture2D);
    Microsoft::WRL::ComPtr<MockD3D11Texture2D> mock_texture(
        new MockD3D11Texture2D());
    return mock_texture.CopyTo(IID_PPV_ARGS(texture2D));
  }

  MOCK_METHOD3(OnCreateTexture2D,
               void(const D3D11_TEXTURE2D_DESC*,
                    const D3D11_SUBRESOURCE_DATA*,
                    ID3D11Texture2D**));

  IFACEMETHODIMP CreateTexture3D(const D3D11_TEXTURE3D_DESC* desc,
                                 const D3D11_SUBRESOURCE_DATA* initial_data,
                                 ID3D11Texture3D** texture2D) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateShaderResourceView(
      ID3D11Resource* resource,
      const D3D11_SHADER_RESOURCE_VIEW_DESC* desc,
      ID3D11ShaderResourceView** srv) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateUnorderedAccessView(
      ID3D11Resource* resource,
      const D3D11_UNORDERED_ACCESS_VIEW_DESC* desc,
      ID3D11UnorderedAccessView** uaview) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateRenderTargetView(
      ID3D11Resource* resource,
      const D3D11_RENDER_TARGET_VIEW_DESC* desc,
      ID3D11RenderTargetView** rtv) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateDepthStencilView(
      ID3D11Resource* resource,
      const D3D11_DEPTH_STENCIL_VIEW_DESC* desc,
      ID3D11DepthStencilView** depth_stencil_view) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateInputLayout(
      const D3D11_INPUT_ELEMENT_DESC* input_element_descs,
      UINT num_elements,
      const void* shader_bytecode,
      SIZE_T bytecode_length,
      ID3D11InputLayout** input_layout) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateVertexShader(const void* shader_bytecode,
                                    SIZE_T bytecode_length,
                                    ID3D11ClassLinkage* class_linkage,
                                    ID3D11VertexShader** vertex_shader) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateGeometryShader(const void* shader_bytecode,
                                      SIZE_T bytecode_length,
                                      ID3D11ClassLinkage* class_linkage,
                                      ID3D11GeometryShader** geometry_shader) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateGeometryShaderWithStreamOutput(
      const void* shader_bytecode,
      SIZE_T bytecode_length,
      const D3D11_SO_DECLARATION_ENTRY* so_declaration,
      UINT num_entries,
      const UINT* buffer_strides,
      UINT num_strides,
      UINT rasterized_stream,
      ID3D11ClassLinkage* class_linkage,
      ID3D11GeometryShader** geometry_shader) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreatePixelShader(const void* shader_bytecode,
                                   SIZE_T bytecode_length,
                                   ID3D11ClassLinkage* class_linkage,
                                   ID3D11PixelShader** pixel_shader) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateHullShader(const void* shader_bytecode,
                                  SIZE_T bytecode_length,
                                  ID3D11ClassLinkage* class_linkage,
                                  ID3D11HullShader** hull_shader) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateDomainShader(const void* shader_bytecode,
                                    SIZE_T bytecode_length,
                                    ID3D11ClassLinkage* class_linkage,
                                    ID3D11DomainShader** domain_shader) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateComputeShader(const void* shader_bytecode,
                                     SIZE_T bytecode_length,
                                     ID3D11ClassLinkage* class_linkage,
                                     ID3D11ComputeShader** compute_shader) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateClassLinkage(ID3D11ClassLinkage** linkage) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateBlendState(const D3D11_BLEND_DESC* blend_state_desc,
                                  ID3D11BlendState** blend_state) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateDepthStencilState(
      const D3D11_DEPTH_STENCIL_DESC* depth_stencil_desc,
      ID3D11DepthStencilState** depth_stencil_state) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateRasterizerState(
      const D3D11_RASTERIZER_DESC* rasterizer_desc,
      ID3D11RasterizerState** rasterizer_state) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateSamplerState(const D3D11_SAMPLER_DESC* sampler_desc,
                                    ID3D11SamplerState** sampler_state) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateQuery(const D3D11_QUERY_DESC* query_desc,
                             ID3D11Query** query) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreatePredicate(const D3D11_QUERY_DESC* predicate_desc,
                                 ID3D11Predicate** predicate) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateCounter(const D3D11_COUNTER_DESC* counter_desc,
                               ID3D11Counter** counter) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CreateDeferredContext(UINT context_flags,
                                       ID3D11DeviceContext** deferred_context) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP OpenSharedResource(HANDLE resource,
                                    REFIID returned_interface,
                                    void** resource_out) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CheckFormatSupport(DXGI_FORMAT format, UINT* format_support) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CheckMultisampleQualityLevels(DXGI_FORMAT format,
                                               UINT sample_count,
                                               UINT* num_quality_levels) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP_(void) CheckCounterInfo(D3D11_COUNTER_INFO* counter_info) {}

  IFACEMETHODIMP CheckCounter(const D3D11_COUNTER_DESC* desc,
                              D3D11_COUNTER_TYPE* type,
                              UINT* active_counters,
                              LPSTR name,
                              UINT* name_length,
                              LPSTR units,
                              UINT* units_length,
                              LPSTR description,
                              UINT* description_length) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP CheckFeatureSupport(D3D11_FEATURE feature,
                                     void* feature_support_data,
                                     UINT feature_support_data_size) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP GetPrivateData(REFGUID guid, UINT* data_size, void* data) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP SetPrivateData(REFGUID guid,
                                UINT data_size,
                                const void* data) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP SetPrivateDataInterface(REFGUID guid, const IUnknown* data) {
    return E_NOTIMPL;
  }

  IFACEMETHODIMP_(D3D_FEATURE_LEVEL) GetFeatureLevel() {
    return D3D_FEATURE_LEVEL_11_1;
  }

  IFACEMETHODIMP_(UINT) GetCreationFlags() { return 0; }

  IFACEMETHODIMP GetDeviceRemovedReason() { return OnGetDeviceRemovedReason(); }

  MOCK_METHOD0(OnGetDeviceRemovedReason, HRESULT());

  IFACEMETHODIMP_(void)
  GetImmediateContext(ID3D11DeviceContext** immediate_context) { return; }

  IFACEMETHODIMP SetExceptionMode(UINT raise_flags) { return E_NOTIMPL; }

  IFACEMETHODIMP_(UINT) GetExceptionMode() { return 0; }

  // Setup default actions for mocked methods
  void SetupDefaultMocks() {
    ON_CALL(*this, OnGetDeviceRemovedReason).WillByDefault([]() {
      return S_OK;
    });
  }

 private:
  ~MockD3D11Device() override = default;
};

class MockVideoCaptureDXGIDeviceManager : public VideoCaptureDXGIDeviceManager {
 public:
  MockVideoCaptureDXGIDeviceManager()
      : VideoCaptureDXGIDeviceManager(nullptr, 0),
        mock_d3d_device_(new MockD3D11Device()) {}

  // Associates a new D3D device with the DXGI Device Manager
  bool ResetDevice() override { return true; }

  // Directly access D3D device stored in DXGI device manager
  Microsoft::WRL::ComPtr<ID3D11Device> GetDevice() override {
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    mock_d3d_device_.As(&device);
    return device;
  }

  Microsoft::WRL::ComPtr<MockD3D11Device> GetMockDevice() {
    return mock_d3d_device_;
  }

 protected:
  ~MockVideoCaptureDXGIDeviceManager() override {}
  Microsoft::WRL::ComPtr<MockD3D11Device> mock_d3d_device_;
};

}  // namespace

class GpuMemoryBufferTrackerTest : public ::testing::Test {
 protected:
  GpuMemoryBufferTrackerTest()
      : media_foundation_supported_(
            VideoCaptureDeviceFactoryWin::PlatformSupportsMediaFoundation()) {}

  bool ShouldSkipTest() {
    if (!media_foundation_supported_) {
      DVLOG(1) << "Media foundation is not supported by the current platform. "
                  "Skipping test.";
      return true;
    }
    // D3D11 is only supported with Media Foundation on Windows 8 or later
    if (base::win::GetVersion() < base::win::Version::WIN8) {
      DVLOG(1) << "D3D11 with Media foundation is not supported by the current "
                  "platform. "
                  "Skipping test.";
      return true;
    }
    return false;
  }

  void SetUp() override {
    if (ShouldSkipTest()) {
      GTEST_SKIP();
    }

    dxgi_device_manager_ = scoped_refptr<MockVideoCaptureDXGIDeviceManager>(
        new MockVideoCaptureDXGIDeviceManager());
  }

  base::test::TaskEnvironment task_environment_;
  const bool media_foundation_supported_;
  scoped_refptr<MockVideoCaptureDXGIDeviceManager> dxgi_device_manager_;
};

TEST_F(GpuMemoryBufferTrackerTest, TextureCreation) {
  // Verify that GpuMemoryBufferTracker creates a D3D11 texture with the correct
  // properties
  const gfx::Size expected_buffer_size = {1920, 1080};
  const DXGI_FORMAT expected_buffer_format = DXGI_FORMAT_NV12;
  dxgi_device_manager_->GetMockDevice()->SetupDefaultMocks();
  EXPECT_CALL(*(dxgi_device_manager_->GetMockDevice().Get()),
              OnCreateTexture2D(
                  Pointee(AllOf(Field(&D3D11_TEXTURE2D_DESC::Format,
                                      expected_buffer_format),
                                Field(&D3D11_TEXTURE2D_DESC::Width,
                                      static_cast<const unsigned int>(
                                          expected_buffer_size.width())),
                                Field(&D3D11_TEXTURE2D_DESC::Height,
                                      static_cast<const unsigned int>(
                                          expected_buffer_size.height())))),
                  _, _));
  std::unique_ptr<VideoCaptureBufferTracker> tracker =
      std::make_unique<GpuMemoryBufferTracker>(dxgi_device_manager_);
  EXPECT_EQ(tracker->Init(expected_buffer_size, PIXEL_FORMAT_NV12, nullptr),
            true);
}

TEST_F(GpuMemoryBufferTrackerTest, TextureRecreationOnDeviceLoss) {
  // Verify that GpuMemoryBufferTracker recreates a D3D11 texture with the
  // correct properties when there is a device loss
  const gfx::Size expected_buffer_size = {1920, 1080};
  const DXGI_FORMAT expected_buffer_format = DXGI_FORMAT_NV12;
  dxgi_device_manager_->GetMockDevice()->SetupDefaultMocks();
  // Expect two texture creation calls (the second occurs on device loss
  // recovery)
  EXPECT_CALL(*(dxgi_device_manager_->GetMockDevice().Get()),
              OnCreateTexture2D(
                  Pointee(AllOf(Field(&D3D11_TEXTURE2D_DESC::Format,
                                      expected_buffer_format),
                                Field(&D3D11_TEXTURE2D_DESC::Width,
                                      static_cast<const unsigned int>(
                                          expected_buffer_size.width())),
                                Field(&D3D11_TEXTURE2D_DESC::Height,
                                      static_cast<const unsigned int>(
                                          expected_buffer_size.height())))),
                  _, _))
      .Times(2);
  // Mock device loss
  EXPECT_CALL(*(dxgi_device_manager_->GetMockDevice().Get()),
              OnGetDeviceRemovedReason())
      .WillOnce(Invoke([]() { return DXGI_ERROR_DEVICE_REMOVED; }));
  // Create and init tracker (causes initial texture creation)
  std::unique_ptr<VideoCaptureBufferTracker> tracker =
      std::make_unique<GpuMemoryBufferTracker>(dxgi_device_manager_);
  EXPECT_EQ(tracker->Init(expected_buffer_size, PIXEL_FORMAT_NV12, nullptr),
            true);
  // Get GpuMemoryBufferHandle (should trigger device/texture recreation)
  gfx::GpuMemoryBufferHandle gmb = tracker->GetGpuMemoryBufferHandle();
}

TEST_F(GpuMemoryBufferTrackerTest, GetMemorySizeInBytes) {
  // Verify that GpuMemoryBufferTracker returns an expected value from
  // GetMemorySizeInBytes
  const gfx::Size expected_buffer_size = {1920, 1080};
  dxgi_device_manager_->GetMockDevice()->SetupDefaultMocks();
  std::unique_ptr<VideoCaptureBufferTracker> tracker =
      std::make_unique<GpuMemoryBufferTracker>(dxgi_device_manager_);
  EXPECT_EQ(tracker->Init(expected_buffer_size, PIXEL_FORMAT_NV12, nullptr),
            true);

  const uint32_t expectedSizeInBytes =
      (expected_buffer_size.width() * expected_buffer_size.height() * 3) / 2;
  EXPECT_EQ(tracker->GetMemorySizeInBytes(), expectedSizeInBytes);
}

}  // namespace media