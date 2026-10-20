// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/copy_shared_image_helper.h"

#include <d3d11.h>
#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/scoped_handle.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/scale.h"
#include "third_party/libyuv/include/libyuv/scale_argb.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"
#include "ui/gl/dc_surface_solid_color_pool.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {
namespace {

// Tightly packed I420 planes.
struct I420Image {
  explicit I420Image(const gfx::Size& size)
      : y(size.GetArea()), u(size.GetArea() / 4), v(size.GetArea() / 4) {}

  std::vector<uint8_t> y;
  std::vector<uint8_t> u;
  std::vector<uint8_t> v;
};

// Fills tightly packed NV12 `data` of `size` with gradients: Y increases with x
// and, more slowly, with y; U increases with x and V decreases with y. All
// three are (up to rounding) affine in x and y, so any linear scaling filter
// reproduces them exactly, while a cropping, flipping or plane mix-up error
// shifts them by well over the tolerance. The values stay away from the limits
// so the YUV->RGB->YUV round trip of the readback doesn't clamp.
void FillNV12Gradient(base::span<uint8_t> data, const gfx::Size& size) {
  const size_t y_plane_size = size.GetArea();
  base::span<uint8_t> y_plane = data.first(y_plane_size);
  base::span<uint8_t> uv_plane = data.subspan(y_plane_size, y_plane_size / 2);
  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      y_plane[y * size.width() + x] =
          static_cast<uint8_t>(48 + (4 * x + 2 * y) / 5);
    }
  }
  const int chroma_width = size.width() / 2;
  for (int cy = 0; cy < size.height() / 2; ++cy) {
    for (int cx = 0; cx < chroma_width; ++cx) {
      const size_t i = static_cast<size_t>(cy * chroma_width + cx) * 2;
      uv_plane[i] = static_cast<uint8_t>(112 + cx / 2);
      uv_plane[i + 1] = static_cast<uint8_t>(144 - cy / 2);
    }
  }
}

// Fills tightly packed RGBA `data` of `size` (top-down rows) with gradients: R
// increases with x, G with y and B decreases with both. See FillNV12Gradient()
// for why gradients are used.
void FillRGBAGradient(base::span<uint8_t> data, const gfx::Size& size) {
  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      base::span<uint8_t> px =
          data.subspan(static_cast<size_t>(y * size.width() + x) * 4, 4u);
      px[0] = static_cast<uint8_t>(2 * x);
      px[1] = static_cast<uint8_t>(2 * y);
      px[2] = static_cast<uint8_t>(255 - x - y);
      px[3] = 255;
    }
  }
}

// Reference for reading back `src_rect` of a tightly packed NV12 image scaled
// to `dst_size`, computed with libyuv. `src_rect` must be even aligned so the
// chroma crop is well defined.
I420Image ScaleNV12WithLibyuv(base::span<const uint8_t> nv12,
                              const gfx::Size& size,
                              const gfx::Rect& src_rect,
                              const gfx::Size& dst_size) {
  CHECK(src_rect.x() % 2 == 0 && src_rect.y() % 2 == 0 &&
        src_rect.width() % 2 == 0 && src_rect.height() % 2 == 0);
  const int stride = size.width();
  const size_t y_plane_size = size.GetArea();
  base::span<const uint8_t> src_y =
      nv12.subspan(static_cast<size_t>(src_rect.y() * stride + src_rect.x()));
  base::span<const uint8_t> src_uv = nv12.subspan(
      y_plane_size +
      static_cast<size_t>((src_rect.y() / 2) * stride + src_rect.x()));

  I420Image cropped(src_rect.size());
  const int crop_width = src_rect.width();
  const int crop_height = src_rect.height();
  CHECK_EQ(0, libyuv::NV12ToI420(src_y.data(), stride, src_uv.data(), stride,
                                 cropped.y.data(), crop_width, cropped.u.data(),
                                 crop_width / 2, cropped.v.data(),
                                 crop_width / 2, crop_width, crop_height));

  I420Image scaled(dst_size);
  CHECK_EQ(0,
           libyuv::I420Scale(
               cropped.y.data(), crop_width, cropped.u.data(), crop_width / 2,
               cropped.v.data(), crop_width / 2, crop_width, crop_height,
               scaled.y.data(), dst_size.width(), scaled.u.data(),
               dst_size.width() / 2, scaled.v.data(), dst_size.width() / 2,
               dst_size.width(), dst_size.height(), libyuv::kFilterBox));
  return scaled;
}

// Reference for reading back `src_rect` of a tightly packed, top-down RGBA
// image scaled to `dst_size` as I420, computed with libyuv. Like the readback,
// this scales in RGB and then converts to limited range BT.601, which is what
// ReadPixelsYUV uses for single plane sources.
I420Image ScaleRGBAToI420WithLibyuv(base::span<const uint8_t> rgba,
                                    const gfx::Size& size,
                                    const gfx::Rect& src_rect,
                                    const gfx::Size& dst_size) {
  const int src_stride = size.width() * 4;
  base::span<const uint8_t> src = rgba.subspan(
      static_cast<size_t>(src_rect.y() * src_stride + src_rect.x() * 4));
  const int dst_stride = dst_size.width() * 4;
  std::vector<uint8_t> scaled(dst_size.GetArea() * 4);
  // ARGBScale() doesn't care about the channel order.
  CHECK_EQ(0, libyuv::ARGBScale(src.data(), src_stride, src_rect.width(),
                                src_rect.height(), scaled.data(), dst_stride,
                                dst_size.width(), dst_size.height(),
                                libyuv::kFilterBox));

  I420Image result(dst_size);
  // RGBA in memory order is what libyuv calls ABGR.
  CHECK_EQ(0, libyuv::ABGRToI420(
                  scaled.data(), dst_stride, result.y.data(), dst_size.width(),
                  result.u.data(), dst_size.width() / 2, result.v.data(),
                  dst_size.width() / 2, dst_size.width(), dst_size.height()));
  return result;
}

void ExpectI420Near(const I420Image& actual,
                    const I420Image& expected,
                    int tolerance) {
  auto check_plane = [&](base::span<const uint8_t> actual_plane,
                         base::span<const uint8_t> expected_plane,
                         const char* name) {
    ASSERT_EQ(actual_plane.size(), expected_plane.size()) << name;
    int max_diff = 0;
    size_t max_diff_index = 0;
    for (size_t i = 0; i < actual_plane.size(); ++i) {
      const int diff = std::abs(actual_plane[i] - expected_plane[i]);
      if (diff > max_diff) {
        max_diff = diff;
        max_diff_index = i;
      }
    }
    EXPECT_LE(max_diff, tolerance)
        << name << " plane differs most at index " << max_diff_index
        << ": actual " << static_cast<int>(actual_plane[max_diff_index])
        << " expected " << static_cast<int>(expected_plane[max_diff_index]);
  };
  check_plane(actual.y, expected.y, "Y");
  check_plane(actual.u, expected.u, "U");
  check_plane(actual.v, expected.v, "V");
}

// Runs the tests with D3D backed shared images on Ganesh, Graphite on D3D11
// and Graphite on D3D12.
class CopySharedImageHelperTest
    : public testing::TestWithParam<std::pair<GrContextType, SkiaBackendType>>,
      public CopySharedImageHelper::ReadbackDelegate {
 public:
  void SetUp() override {
    surface_ = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplayEGL(),
                                                  gfx::Size());
    ASSERT_TRUE(surface_);
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    ASSERT_TRUE(context_);
    ASSERT_TRUE(context_->MakeCurrent(surface_.get()));
    dawnProcSetProcs(&dawn::native::GetProcs());

    auto [gr_context_type, skia_backend_type] = GetParam();
    if (gr_context_type == GrContextType::kGraphiteDawn) {
      if (skia_backend_type == SkiaBackendType::kGraphiteDawnD3D12) {
        feature_list_.InitWithFeatures({features::kDCompOnD3D12}, {});
        // Read by DawnContextProvider::GetDefaultBackendType(). The command
        // line is restored between tests, so this doesn't affect other params.
        base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
            switches::kSkiaGraphiteDawnBackend,
            switches::kSkiaGraphiteDawnBackendD3D12);
      }
      features::InitSkiaGraphiteDefaultParamsForTesting();
      dawn_context_provider_ =
          DawnContextProvider::Create(GpuPreferences(), GpuFeatureInfo());
    }

    context_state_ = base::MakeRefCounted<SharedContextState>(
        base::MakeRefCounted<gl::GLShareGroup>(), surface_, context_,
        /*use_virtualized_gl_contexts=*/false, base::DoNothing(),
        gr_context_type, /*vulkan_context_provider=*/nullptr,
        dawn_context_provider_.get());

    // On Graphite D3D12 this is a D3D11On12 device.
    auto d3d11_device = context_state_->GetD3D11Device();
    ASSERT_TRUE(d3d11_device);

    // Needed for the SCANOUT usage of the test images, see
    // GetDisplayReadUsage().
    gl::InitializeDirectComposition(
        d3d11_device, context_state_->GetD3D12CommandQueue(),
        gl::CreateDCSurfaceSolidColorPoolFactory(d3d11_device));
    if (context_state_->IsGraphiteDawnD3D12() &&
        (!gl::DirectCompositionTextureSupported() ||
         !gl::GetDirectCompositionD3D12CommandQueue())) {
      GTEST_SKIP() << "IDCompositionTexture backed by a D3D12 resource is "
                      "not supported on this machine";
    }

    shared_image_factory_ = std::make_unique<D3DImageBackingFactory>(
        d3d11_device, shared_image_manager_.dxgi_shared_handle_manager(),
        GLFormatCaps());
    context_state_->InitializeSkia(GpuPreferences(), workarounds_);
    context_state_->InitializeGL(GpuPreferences(), workarounds_,
                                 GpuFeatureInfo());
    helper_ = std::make_unique<CopySharedImageHelper>(&representation_factory_,
                                                      context_state_.get());
  }

  void TearDown() override {
    // Destroy the shared images while their context is still alive, and
    // dawn_context_provider_ before resetting dawn's proc table.
    helper_.reset();
    factory_refs_.clear();
    context_state_ = nullptr;
    dawn_context_provider_ = nullptr;
    gl::ShutdownDirectComposition();
    dawnProcSetProcs(nullptr);
  }

  // CopySharedImageHelper::ReadbackDelegate implementation, mirroring
  // RasterDecoder.
  const GpuDriverBugWorkarounds& workarounds() const override {
    return workarounds_;
  }
  bool WasContextLost() const override {
    return context_state_->context_lost();
  }
  bool CheckResetStatus() override {
    return context_state_->CheckResetStatus(/*needs_gl=*/false);
  }
  void DoFinish() override {
    context_state_->FlushAndSubmit(/*sync_to_cpu=*/true);
  }

 protected:
  SharedImageUsageSet GetDisplayReadUsage() const {
    // SCANOUT is needed for D3D12 to trigger DComp texture path which creates a
    // shared handle required for cross-device (D3D11->D3D12) resource sharing.
    return SHARED_IMAGE_USAGE_DISPLAY_READ |
           (context_state_->IsGraphiteDawnD3D12() ? SHARED_IMAGE_USAGE_SCANOUT
                                                  : SharedImageUsageSet());
  }

  // Registers `backing` with the shared image manager and returns its mailbox.
  // The ref is kept alive until the fixture is torn down.
  Mailbox Register(std::unique_ptr<SharedImageBacking> backing) {
    const Mailbox mailbox = backing->mailbox();
    factory_refs_.push_back(
        shared_image_manager_.Register(std::move(backing), &tracker_));
    return mailbox;
  }

  // Creates an NV12 shared image with tightly packed `nv12_data` the way video
  // capture and decoder frames are created: a shared handle D3D11 texture with
  // a keyed mutex, imported through the backing factory.
  Mailbox CreateNV12Image(const gfx::Size& size,
                          base::span<const uint8_t> nv12_data,
                          const gfx::ColorSpace& color_space) {
    CHECK_EQ(nv12_data.size(), static_cast<size_t>(size.GetArea()) * 3 / 2);
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        shared_image_factory_->GetDeviceForTesting();

    D3D11_SUBRESOURCE_DATA data = {
        .pSysMem = nv12_data.data(),
        .SysMemPitch = static_cast<UINT>(size.width())};
    // Same bind flags as the NV12 textures produced by video capture.
    CD3D11_TEXTURE2D_DESC desc(
        DXGI_FORMAT_NV12, size.width(), size.height(), 1, 1,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                     D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
    CHECK_EQ(S_OK, d3d11_device->CreateTexture2D(&desc, &data, &d3d11_texture));
    // The initial data upload is only queued on this device's immediate
    // context; submit it before another device (e.g. Dawn on D3D12) reads the
    // texture through the shared handle.
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
    d3d11_device->GetImmediateContext(&device_context);
    device_context->Flush();

    Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
    CHECK_EQ(S_OK, d3d11_texture.As(&dxgi_resource));
    HANDLE handle = nullptr;
    CHECK_EQ(S_OK, dxgi_resource->CreateSharedHandle(
                       nullptr,
                       DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
                       nullptr, &handle));

    // The usages video frames are created with.
    auto backing = shared_image_factory_->CreateSharedImage(
        Mailbox::Generate(),
        {viz::MultiPlaneFormat::kNV12, size, color_space,
         kTopLeft_GrSurfaceOrigin, kUnpremul_SkAlphaType,
         SHARED_IMAGE_USAGE_VIDEO_DECODE | SHARED_IMAGE_USAGE_GLES2_READ |
             SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_DISPLAY_READ |
             SHARED_IMAGE_USAGE_SCANOUT | SHARED_IMAGE_USAGE_WEBGPU_READ,
         "TestLabel"},
        /*is_thread_safe=*/false,
        gfx::GpuMemoryBufferHandle(
            gfx::DXGIHandle(base::win::ScopedHandle(handle))));
    CHECK(backing);
    return Register(std::move(backing));
  }

  // Reads back `src_rect` of `mailbox` scaled to `dst_size` with
  // CopySharedImageHelper::ReadPixelsYUV() into padded planes, to catch code
  // that assumes tightly packed planes, and repacks them into `out`. Returns
  // false if the readback failed.
  bool ReadPixelsYUV(const Mailbox& mailbox,
                     const gfx::Rect& src_rect,
                     const gfx::Size& dst_size,
                     I420Image* out) {
    auto source_shared_image =
        representation_factory_.ProduceSkia(mailbox, context_state_);
    if (!source_shared_image) {
      return false;
    }
    const int width = dst_size.width();
    const int height = dst_size.height();
    const int y_stride = width + 16;
    const int uv_stride = width / 2 + 8;
    std::vector<uint8_t> y(y_stride * height);
    std::vector<uint8_t> u(uv_stride * height / 2);
    std::vector<uint8_t> v(uv_stride * height / 2);
    auto result = helper_->ReadPixelsYUV(
        this, src_rect.x(), src_rect.y(), src_rect.width(), src_rect.height(),
        width, height, y.data(), y_stride, u.data(), uv_stride, v.data(),
        uv_stride, std::move(source_shared_image));
    if (!result.has_value()) {
      ADD_FAILURE() << result.error().function_name << ": "
                    << result.error().msg;
      return false;
    }
    libyuv::I420Copy(y.data(), y_stride, u.data(), uv_stride, v.data(),
                     uv_stride, out->y.data(), width, out->u.data(), width / 2,
                     out->v.data(), width / 2, width, height);
    return true;
  }

  void RunReadPixelsYUVRGBATest(GrSurfaceOrigin origin);

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  SharedImageManager shared_image_manager_;
  MemoryTypeTracker tracker_{nullptr};
  SharedImageRepresentationFactory representation_factory_{
      &shared_image_manager_, nullptr};
  std::vector<std::unique_ptr<SharedImageRepresentationFactoryRef>>
      factory_refs_;
  std::unique_ptr<D3DImageBackingFactory> shared_image_factory_;
  std::unique_ptr<DawnContextProvider> dawn_context_provider_;
  scoped_refptr<SharedContextState> context_state_;
  GpuDriverBugWorkarounds workarounds_;
  std::unique_ptr<CopySharedImageHelper> helper_;
  base::test::ScopedFeatureList feature_list_;
};

// Reads back a D3D backed NV12 shared image and compares the result against
// libyuv scaling the same source.
TEST_P(CopySharedImageHelperTest, ReadPixelsYUVNV12) {
  // TODO: Investigate why the readback fails for NV12 images on Ganesh.
  if (!context_state_->IsGraphiteDawn()) {
    GTEST_SKIP() << "NV12 readback fails on Ganesh";
  }
  constexpr gfx::Size kSrcSize(128, 128);
  // The readback converts YUV->RGB, scales and converts back; the reference
  // scales in YUV. Both are exact for the gradients up to rounding.
  constexpr int kTolerance = 4;

  std::vector<uint8_t> nv12(kSrcSize.GetArea() * 3 / 2);
  FillNV12Gradient(nv12, kSrcSize);
  // A real YUV color space is needed for the YUV->RGB->YUV round trip to
  // reproduce the input.
  const Mailbox mailbox =
      CreateNV12Image(kSrcSize, nv12, gfx::ColorSpace::CreateREC709());

  struct Case {
    gfx::Rect src_rect;
    gfx::Size dst_size;
  };
  const Case kCases[] = {
      // Whole image, 1:1, 2x and 8x downscaled, and anisotropic 2x / 4x.
      {gfx::Rect(kSrcSize), gfx::Size(128, 128)},
      {gfx::Rect(kSrcSize), gfx::Size(64, 64)},
      {gfx::Rect(kSrcSize), gfx::Size(16, 16)},
      {gfx::Rect(kSrcSize), gfx::Size(64, 32)},
      // Sub rects, 1:1 and 2x downscaled.
      {gfx::Rect(2, 2, 124, 124), gfx::Size(124, 124)},
      {gfx::Rect(32, 16, 64, 96), gfx::Size(32, 48)},
  };
  for (const auto& [src_rect, dst_size] : kCases) {
    SCOPED_TRACE(testing::Message() << "src_rect=" << src_rect.ToString()
                                    << " dst_size=" << dst_size.ToString());
    I420Image result(dst_size);
    ASSERT_TRUE(ReadPixelsYUV(mailbox, src_rect, dst_size, &result));
    ExpectI420Near(result,
                   ScaleNV12WithLibyuv(nv12, kSrcSize, src_rect, dst_size),
                   kTolerance);
  }
}

// Reads back an RGBA shared image of the given surface origin and compares
// against libyuv. For a bottom left origin image (e.g. a WebGL back buffer) the
// texture rows are stored bottom-up, so the readback has to flip them and
// interpret `src_rect` in top-down logical coordinates, also when `src_rect` is
// a scaled sub rect (crbug.com/570097637).
void CopySharedImageHelperTest::RunReadPixelsYUVRGBATest(
    GrSurfaceOrigin origin) {
  constexpr gfx::Size kSrcSize(128, 128);
  // Skia and libyuv use slightly different RGB->YUV fixed point arithmetic.
  constexpr int kTolerance = 4;

  // The logical, top-down image that the reference is computed from.
  std::vector<uint8_t> logical_data(kSrcSize.GetArea() * 4);
  FillRGBAGradient(logical_data, kSrcSize);
  // With a bottom left origin the first memory row is the bottom of the image.
  std::vector<uint8_t> pixel_data = logical_data;
  if (origin == kBottomLeft_GrSurfaceOrigin) {
    const size_t row_bytes = kSrcSize.width() * 4;
    for (int row = 0; row < kSrcSize.height(); ++row) {
      const size_t src_row = kSrcSize.height() - 1 - row;
      std::ranges::copy(
          base::span(logical_data).subspan(src_row * row_bytes, row_bytes),
          base::span(pixel_data).subspan(row * row_bytes, row_bytes).begin());
    }
  }

  auto backing = shared_image_factory_->CreateSharedImage(
      Mailbox::Generate(),
      {viz::SinglePlaneFormat::kRGBA_8888, kSrcSize,
       gfx::ColorSpace::CreateSRGB(), origin, kPremul_SkAlphaType,
       GetDisplayReadUsage() | SHARED_IMAGE_USAGE_RASTER_READ, "TestLabel"},
      /*is_thread_safe=*/false, base::span<const uint8_t>(pixel_data));
  ASSERT_NE(backing, nullptr);
  const Mailbox mailbox = Register(std::move(backing));

  struct Case {
    gfx::Rect src_rect;  // Logical, top-down coordinates.
    gfx::Size dst_size;
  };
  const Case kCases[] = {
      // Whole image, 1:1 and 4x downscaled.
      {gfx::Rect(kSrcSize), gfx::Size(128, 128)},
      {gfx::Rect(kSrcSize), gfx::Size(32, 32)},
      // Top left quadrant, 1:1.
      {gfx::Rect(0, 0, 64, 64), gfx::Size(64, 64)},
      // Bottom left quadrant, 2x downscaled.
      {gfx::Rect(0, 64, 64, 64), gfx::Size(32, 32)},
      // Off center sub rect, anisotropic 2x / 4x downscaled.
      {gfx::Rect(24, 40, 80, 64), gfx::Size(40, 16)},
  };
  for (const auto& [src_rect, dst_size] : kCases) {
    SCOPED_TRACE(testing::Message() << "src_rect=" << src_rect.ToString()
                                    << " dst_size=" << dst_size.ToString());
    I420Image result(dst_size);
    ASSERT_TRUE(ReadPixelsYUV(mailbox, src_rect, dst_size, &result));
    ExpectI420Near(
        result,
        ScaleRGBAToI420WithLibyuv(logical_data, kSrcSize, src_rect, dst_size),
        kTolerance);
  }
}

TEST_P(CopySharedImageHelperTest, ReadPixelsYUVTopLeftOrigin) {
  RunReadPixelsYUVRGBATest(kTopLeft_GrSurfaceOrigin);
}

TEST_P(CopySharedImageHelperTest, ReadPixelsYUVBottomLeftOrigin) {
  // TODO(crbug.com/570097637): Graphite reads the wrong rows when scaling a
  // sub rect of a bottom left origin image.
  if (context_state_->IsGraphiteDawn()) {
    GTEST_SKIP() << "Scaled sub rect readback is broken on Graphite";
  }
  RunReadPixelsYUVRGBATest(kBottomLeft_GrSurfaceOrigin);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CopySharedImageHelperTest,
    ::testing::Values(std::make_pair(GrContextType::kGL,
                                     SkiaBackendType::kUnknown),
                      std::make_pair(GrContextType::kGraphiteDawn,
                                     SkiaBackendType::kGraphiteDawnD3D11),
                      std::make_pair(GrContextType::kGraphiteDawn,
                                     SkiaBackendType::kGraphiteDawnD3D12)),
    [](const testing::TestParamInfo<std::pair<GrContextType, SkiaBackendType>>&
           info) {
      switch (info.param.second) {
        case SkiaBackendType::kGraphiteDawnD3D11:
          return "GraphiteDawnD3D11";
        case SkiaBackendType::kGraphiteDawnD3D12:
          return "GraphiteDawnD3D12";
        default:
          return "GaneshGL";
      }
    });

}  // namespace
}  // namespace gpu
