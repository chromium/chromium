// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dcomp_image_backing_factory.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/test/sk_color_eq.h"
#include "ui/gl/child_window_win.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_test_helper.h"
#include "ui/platform_window/win/win_window.h"

namespace gpu {

namespace {

constexpr SharedImageUsageSet kDXGISwapChainUsage =
    SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE |
    SHARED_IMAGE_USAGE_SCANOUT | SHARED_IMAGE_USAGE_SCANOUT_DXGI_SWAP_CHAIN;

constexpr SharedImageUsageSet kDCompSurfaceUsage =
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_SCANOUT |
    SHARED_IMAGE_USAGE_SCANOUT_DCOMP_SURFACE;

constexpr GrContextType kGrContextTypeDontCare = GrContextType::kGL;

}  // namespace

class DCompImageBackingFactoryTest : public testing::Test {
 public:
  void SetUp() override {
    surface_ = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplayEGL(),
                                                  gfx::Size());
    ASSERT_TRUE(surface_);
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    ASSERT_TRUE(context_);
    bool result = context_->MakeCurrent(surface_.get());
    ASSERT_TRUE(result);

    GpuDriverBugWorkarounds workarounds;
    scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();
    context_state_ = base::MakeRefCounted<SharedContextState>(
        std::move(share_group), surface_, context_,
        /*use_virtualized_gl_contexts=*/false, base::DoNothing(),
        GrContextType::kGL);
    context_state_->InitializeSkia(GpuPreferences(), workarounds);
    auto feature_info =
        base::MakeRefCounted<gles2::FeatureInfo>(workarounds, GpuFeatureInfo());
    context_state_->InitializeGL(GpuPreferences(), std::move(feature_info));

    d3d11_device_ = gl::QueryD3D11DeviceObjectFromANGLE();

    memory_type_tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
    shared_image_representation_factory_ =
        std::make_unique<SharedImageRepresentationFactory>(
            &shared_image_manager_, nullptr);
    shared_image_factory_ =
        std::make_unique<DCompImageBackingFactory>(context_state_);
  }

 protected:
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<SharedContextState> context_state_;

  SharedImageManager shared_image_manager_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  std::unique_ptr<DCompImageBackingFactory> shared_image_factory_;

  void RunDXGISwapChainAlphaTest(bool has_alpha) {
    Mailbox mailbox = Mailbox::Generate();
    std::unique_ptr<SharedImageBacking> backing =
        shared_image_factory_->CreateSharedImage(
            mailbox, viz::SinglePlaneFormat::kRGBA_8888, nullptr,
            gfx::Size(100, 100), gfx::ColorSpace::CreateSRGB(),
            kTopLeft_GrSurfaceOrigin,
            has_alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType,
            kDXGISwapChainUsage, "TestLabel", false);
    ASSERT_NE(nullptr, backing);
    std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
        shared_image_manager_.Register(std::move(backing),
                                       memory_type_tracker_.get());

    shared_image_representation_factory_->ProduceSkia(mailbox, context_state_)
        ->SetCleared();

    Microsoft::WRL::ComPtr<IUnknown> content =
        shared_image_representation_factory_->ProduceOverlay(mailbox)
            ->BeginScopedReadAccess()
            ->GetDCLayerOverlayImage()
            ->dcomp_visual_content();
    ASSERT_NE(nullptr, content);

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;
    ASSERT_HRESULT_SUCCEEDED(content.As(&swap_chain));
    DXGI_SWAP_CHAIN_DESC1 desc;
    ASSERT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
    if (has_alpha) {
      EXPECT_EQ(DXGI_ALPHA_MODE_PREMULTIPLIED, desc.AlphaMode);
    } else {
      EXPECT_EQ(DXGI_ALPHA_MODE_IGNORE, desc.AlphaMode);
    }
  }
};

TEST_F(DCompImageBackingFactoryTest, UsageFlags) {
  EXPECT_TRUE(shared_image_factory_->CanCreateSharedImage(
      kDCompSurfaceUsage, viz::SinglePlaneFormat::kRGBA_8888,
      gfx::Size(100, 100), false, gfx::GpuMemoryBufferType::EMPTY_BUFFER,
      kGrContextTypeDontCare, {}));

  EXPECT_TRUE(shared_image_factory_->CanCreateSharedImage(
      kDXGISwapChainUsage, viz::SinglePlaneFormat::kRGBA_8888,
      gfx::Size(100, 100), false, gfx::GpuMemoryBufferType::EMPTY_BUFFER,
      kGrContextTypeDontCare, {}));

  // DComp surfaces don't support readback.
  EXPECT_FALSE(shared_image_factory_->CanCreateSharedImage(
      kDCompSurfaceUsage | SHARED_IMAGE_USAGE_DISPLAY_READ,
      viz::SinglePlaneFormat::kRGBA_8888, gfx::Size(100, 100), false,
      gfx::GpuMemoryBufferType::EMPTY_BUFFER, kGrContextTypeDontCare, {}));

  // We require callers to explicitly state DXGI swap chains are readable.
  EXPECT_FALSE(shared_image_factory_->CanCreateSharedImage(
      SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_SCANOUT,
      viz::SinglePlaneFormat::kRGBA_8888, gfx::Size(100, 100), false,
      gfx::GpuMemoryBufferType::EMPTY_BUFFER, kGrContextTypeDontCare, {}));
}

TEST_F(DCompImageBackingFactoryTest, HDR10Support) {
  EXPECT_TRUE(shared_image_factory_->CanCreateSharedImage(
      kDXGISwapChainUsage, viz::SinglePlaneFormat::kRGBA_1010102,
      gfx::Size(100, 100), false, gfx::GpuMemoryBufferType::EMPTY_BUFFER,
      kGrContextTypeDontCare, {}));

  EXPECT_FALSE(shared_image_factory_->CanCreateSharedImage(
      kDCompSurfaceUsage, viz::SinglePlaneFormat::kRGBA_1010102,
      gfx::Size(100, 100), false, gfx::GpuMemoryBufferType::EMPTY_BUFFER,
      kGrContextTypeDontCare, {}));
}

TEST_F(DCompImageBackingFactoryTest, ValidFormats) {
  SharedImageUsageSet valid_usages[2] = {kDCompSurfaceUsage,
                                         kDXGISwapChainUsage};

  viz::SharedImageFormat valid_formats[5] = {
      viz::SinglePlaneFormat::kRGBA_8888, viz::SinglePlaneFormat::kBGRA_8888,
      viz::SinglePlaneFormat::kRGBX_8888, viz::SinglePlaneFormat::kBGRX_8888,
      viz::SinglePlaneFormat::kRGBA_F16,
  };

  for (auto valid_usage : valid_usages) {
    for (auto valid_format : valid_formats) {
      // We don't support sharing memory
      EXPECT_TRUE(shared_image_factory_->CanCreateSharedImage(
          valid_usage, valid_format, gfx::Size(100, 100), false,
          gfx::GpuMemoryBufferType::EMPTY_BUFFER, kGrContextTypeDontCare, {}))
          << "usage = " << CreateLabelForSharedImageUsage(valid_usage)
          << ", format = " << valid_format.ToString();
    }
  }
}

// Test that |asyncRescaleAndReadPixels| works on a DXGI swap chain-backed
// SharedImage for CopyOutput support.
TEST_F(DCompImageBackingFactoryTest, CanReadDXGISwapChain) {
  Mailbox mailbox = Mailbox::Generate();
  std::unique_ptr<SharedImageBacking> backing =
      shared_image_factory_->CreateSharedImage(
          mailbox, viz::SinglePlaneFormat::kRGBA_8888, nullptr,
          gfx::Size(100, 100), gfx::ColorSpace::CreateSRGB(),
          kTopLeft_GrSurfaceOrigin, kOpaque_SkAlphaType, kDXGISwapChainUsage,
          "TestLabel", false);
  ASSERT_NE(nullptr, backing);
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  std::unique_ptr<SkiaImageRepresentation> skia_representation =
      shared_image_representation_factory_->ProduceSkia(mailbox,
                                                        context_state_);
  ASSERT_NE(nullptr, skia_representation);

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess> write_access =
      skia_representation->BeginScopedWriteAccess(
          &begin_semaphores, &end_semaphores,
          SkiaImageRepresentation::AllowUnclearedAccess::kYes);
  ASSERT_NE(nullptr, write_access);

  write_access->surface()->getCanvas()->clear(SK_ColorRED);

  EXPECT_EQ(0u, end_semaphores.size());
  GrFlushInfo flush_info;
  GrDirectContext* direct_context = context_state_->gr_context();
  EXPECT_EQ(
      GrSemaphoresSubmitted::kYes,
      direct_context->flush(write_access->surface(), flush_info, nullptr));
  skia_representation->SetCleared();

  std::unique_ptr<const SkImage::AsyncReadResult> readback_result;
  write_access->surface()->asyncRescaleAndReadPixels(
      SkImageInfo::MakeN32Premul(100, 100), SkIRect::MakeXYWH(25, 25, 1, 1),
      SkSurface::RescaleGamma::kLinear, SkSurface::RescaleMode::kNearest,
      [](void* context,
         std::unique_ptr<const SkImage::AsyncReadResult> result) {
        *reinterpret_cast<std::unique_ptr<const SkImage::AsyncReadResult>*>(
            context) = std::move(result);
      },
      &readback_result);
  direct_context->submit(GrSyncCpu::kYes);

  ASSERT_NE(nullptr, readback_result);
  EXPECT_EQ(1, readback_result->count());
  EXPECT_EQ(SK_ColorRED,
            *reinterpret_cast<const SkColor*>(readback_result->data(0)));
}

// The DComp surface backing binds to the GL default framebuffer. This ensures
// that we correctly restore the previous current surface after we're done
// drawing.
TEST_F(DCompImageBackingFactoryTest, DCompSurfaceRestoresGLSurfaceAfterDraw) {
  Mailbox mailbox = Mailbox::Generate();
  std::unique_ptr<SharedImageBacking> backing =
      shared_image_factory_->CreateSharedImage(
          mailbox, viz::SinglePlaneFormat::kRGBA_8888, nullptr,
          gfx::Size(100, 100), gfx::ColorSpace::CreateSRGB(),
          kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, kDCompSurfaceUsage,
          "TestLabel", false);
  ASSERT_NE(nullptr, backing);
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  std::unique_ptr<SkiaImageRepresentation> skia_representation =
      shared_image_representation_factory_->ProduceSkia(mailbox,
                                                        context_state_);

  gl::GLSurface* original_surface = gl::GLSurface::GetCurrent();
  EXPECT_NE(nullptr, original_surface);
  {
    std::vector<GrBackendSemaphore> begin_semaphores;
    std::vector<GrBackendSemaphore> end_semaphores;
    std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess> write_access =
        skia_representation->BeginScopedWriteAccess(
            &begin_semaphores, &end_semaphores,
            SkiaImageRepresentation::AllowUnclearedAccess::kYes);
    EXPECT_NE(nullptr, write_access);

    // DCompImageBacking has its own GLSurface that wraps the DComp surface
    // update texture.
    EXPECT_NE(gl::GLSurface::GetCurrent(), original_surface);
  }
  EXPECT_EQ(gl::GLSurface::GetCurrent(), original_surface);
}

// The DComp surface returns a "surface serial" to indicate that its contents
// have changed (since we need to Commit the DComp tree to reflect the surface
// changes). This test ensures that this value changes after draws.
TEST_F(DCompImageBackingFactoryTest,
       DCompSurfaceMultipleDrawsIncrementSurfaceSerial) {
  Mailbox mailbox = Mailbox::Generate();
  std::unique_ptr<SharedImageBacking> backing =
      shared_image_factory_->CreateSharedImage(
          mailbox, viz::SinglePlaneFormat::kRGBA_8888, nullptr,
          gfx::Size(100, 100), gfx::ColorSpace::CreateSRGB(),
          kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, kDCompSurfaceUsage,
          "TestLabel", false);
  ASSERT_NE(nullptr, backing);
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  std::unique_ptr<SkiaImageRepresentation> skia_representation =
      shared_image_representation_factory_->ProduceSkia(mailbox,
                                                        context_state_);

  // Force cleared, so we can get an overlay read access
  skia_representation->SetCleared();

  uint64_t previous_serial =
      shared_image_representation_factory_->ProduceOverlay(mailbox)
          ->BeginScopedReadAccess()
          ->GetDCLayerOverlayImage()
          ->dcomp_surface_serial();

  for (int i = 0; i < 10; i++) {
    {
      std::vector<GrBackendSemaphore> begin_semaphores;
      std::vector<GrBackendSemaphore> end_semaphores;
      std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess> write_access =
          skia_representation->BeginScopedWriteAccess(
              &begin_semaphores, &end_semaphores,
              SkiaImageRepresentation::AllowUnclearedAccess::kYes);
      EXPECT_NE(nullptr, write_access);
    }

    uint64_t current_serial =
        shared_image_representation_factory_->ProduceOverlay(mailbox)
            ->BeginScopedReadAccess()
            ->GetDCLayerOverlayImage()
            ->dcomp_surface_serial();

    // We only care that the previous serial is not the same as the previous
    EXPECT_NE(current_serial, previous_serial);

    previous_serial = current_serial;
  }
}

// Ensure that creating a DXGI swap chain SI, we get a swap chain with the right
// alpha mode.
TEST_F(DCompImageBackingFactoryTest, DXGISwapChainAlphaOpaque) {
  RunDXGISwapChainAlphaTest(/*has_alpha=*/false);
}

TEST_F(DCompImageBackingFactoryTest, DXGISwapChainAlphaPremultiplied) {
  RunDXGISwapChainAlphaTest(/*has_alpha=*/true);
}

class DCompImageBackingFactoryBufferCountTest
    : public DCompImageBackingFactoryTest,
      public testing::WithParamInterface<bool> {
 public:
  static const char* GetParamName(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "DCompTripleBufferRootSwapChain" : "default";
  }

 protected:
  void SetUp() override {
    if (GetParam()) {
      enabled_features_.InitWithFeatures(
          {features::kDCompTripleBufferRootSwapChain}, {});
    } else {
      enabled_features_.InitWithFeatures(
          {}, {features::kDCompTripleBufferRootSwapChain});
    }

    DCompImageBackingFactoryTest::SetUp();
  }

  base::test::ScopedFeatureList enabled_features_;
};

TEST_P(DCompImageBackingFactoryBufferCountTest, RootSwapChainBufferCount) {
  Mailbox mailbox = Mailbox::Generate();
  std::unique_ptr<SharedImageBacking> backing =
      shared_image_factory_->CreateSharedImage(
          mailbox, viz::SinglePlaneFormat::kRGBA_8888, nullptr,
          gfx::Size(100, 100), gfx::ColorSpace::CreateSRGB(),
          kTopLeft_GrSurfaceOrigin, kOpaque_SkAlphaType, kDXGISwapChainUsage,
          "TestLabel", false);
  ASSERT_NE(nullptr, backing);
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  shared_image_representation_factory_->ProduceSkia(mailbox, context_state_)
      ->SetCleared();

  Microsoft::WRL::ComPtr<IUnknown> content =
      shared_image_representation_factory_->ProduceOverlay(mailbox)
          ->BeginScopedReadAccess()
          ->GetDCLayerOverlayImage()
          ->dcomp_visual_content();

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;
  ASSERT_HRESULT_SUCCEEDED(content.As(&swap_chain));
  DXGI_SWAP_CHAIN_DESC1 desc;
  ASSERT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  if (GetParam()) {
    EXPECT_EQ(3u, desc.BufferCount);
  } else {
    EXPECT_EQ(2u, desc.BufferCount);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DCompImageBackingFactoryBufferCountTest,
    testing::Bool(),
    &DCompImageBackingFactoryBufferCountTest::GetParamName);

class DCompImageBackingFactoryVisualTreeTest
    : public DCompImageBackingFactoryTest {
 protected:
  DCompImageBackingFactoryVisualTreeTest()
      : window_size_(100, 100),
        window_(&platform_delegate_, gfx::Rect(window_size_)),
        dcomp_device_(gl::GetDirectCompositionDevice()) {}

  void SetUp() override {
    DCompImageBackingFactoryTest::SetUp();

    static_cast<ui::PlatformWindow*>(&window_)->Show();
    child_window_.Initialize();
    child_window_.Resize(window_size_);
    ::SetParent(child_window_.window(), window_.hwnd());
  }

  // This does not need to be called unless the test expects alpha blending.
  void set_background_fill_override(const SkColor4f& background_fill_override) {
    background_fill_override_ = background_fill_override;
  }

  void InitializeVisualTreeWithContent(IUnknown* content) {
    Microsoft::WRL::ComPtr<IDCompositionDesktopDevice> desktop_device;
    ASSERT_HRESULT_SUCCEEDED(dcomp_device_.As(&desktop_device));

    ASSERT_HRESULT_SUCCEEDED(desktop_device->CreateTargetForHwnd(
        child_window_.window(), TRUE, &dcomp_target_));

    ASSERT_HRESULT_SUCCEEDED(dcomp_device_->CreateVisual(&dcomp_root_visual_));
    ASSERT_NE(dcomp_root_visual_, nullptr);
    ASSERT_HRESULT_SUCCEEDED(dcomp_target_->SetRoot(dcomp_root_visual_.Get()));

    // Fill the background so we have a consistent backdrop instead of relying
    // on the color of the redirection surface when testing alpha blending. We
    // default to magenta to make it obvious when something shouldn't be
    // visible.
    const SkColor4f background_fill_color =
        background_fill_override_.value_or(SkColors::kMagenta);
    {
      Microsoft::WRL::ComPtr<IDCompositionSurface> background_fill;
      ASSERT_HRESULT_SUCCEEDED(dcomp_device_->CreateSurface(
          window_size_.width(), window_size_.height(),
          DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ALPHA_MODE_IGNORE,
          &background_fill));

      RECT update_rect = gfx::Rect(window_size_).ToRECT();
      Microsoft::WRL::ComPtr<ID3D11Texture2D> update_texture;
      POINT update_offset;
      ASSERT_HRESULT_SUCCEEDED(background_fill->BeginDraw(
          &update_rect, IID_PPV_ARGS(&update_texture), &update_offset));
      {
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target;
        ASSERT_HRESULT_SUCCEEDED(d3d11_device_->CreateRenderTargetView(
            update_texture.Get(), nullptr, &render_target));
        ASSERT_NE(render_target, nullptr);
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
        d3d11_device_->GetImmediateContext(&d3d11_device_context);
        ASSERT_NE(d3d11_device_context, nullptr);
        d3d11_device_context->ClearRenderTargetView(
            render_target.Get(), background_fill_color.vec());
      }
      ASSERT_HRESULT_SUCCEEDED(background_fill->EndDraw());

      // The content of a visual is always drawn behind its children, so we'll
      // use it for the background fill.
      ASSERT_HRESULT_SUCCEEDED(
          dcomp_root_visual_->SetContent(background_fill.Get()));
    }

    Microsoft::WRL::ComPtr<IDCompositionVisual2> content_visual;
    ASSERT_HRESULT_SUCCEEDED(dcomp_device_->CreateVisual(&content_visual));
    ASSERT_HRESULT_SUCCEEDED(content_visual->SetContent(content));
    ASSERT_HRESULT_SUCCEEDED(
        dcomp_root_visual_->AddVisual(content_visual.Get(), FALSE, nullptr));
  }

  void CommitAndWait() {
    ASSERT_HRESULT_SUCCEEDED(dcomp_device_->Commit());
    ASSERT_HRESULT_SUCCEEDED(dcomp_device_->WaitForCommitCompletion());

    // Wait for DXGI swap chains to flip, just in case.
    Sleep(1000);
  }

  HWND window() const { return child_window_.window(); }

  void FillAreaAndSubmit(SkiaImageRepresentation* skia_representation,
                         const gfx::Rect& update_rect,
                         SkColor color) {
    std::vector<GrBackendSemaphore> begin_semaphores;
    std::vector<GrBackendSemaphore> end_semaphores;
    std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess> write_access =
        skia_representation->BeginScopedWriteAccess(
            1, SkSurfaceProps(), update_rect, &begin_semaphores,
            &end_semaphores,
            SkiaImageRepresentation::AllowUnclearedAccess::kYes, true);
    ASSERT_NE(nullptr, write_access);

    auto* canvas = write_access->surface()->getCanvas();

    // Clear |update_rect| instead of drawing, since drawing does alpha blending
    // and depends on initialized pixels in |update_rect|, which we might not
    // have.
    canvas->save();
    canvas->clipRect(gfx::RectToSkRect(update_rect));
    canvas->clear(color);
    canvas->restore();

    EXPECT_EQ(0u, end_semaphores.size());
    GrFlushInfo flush_info;
    GrDirectContext* direct_context = context_state_->gr_context();
    EXPECT_EQ(
        GrSemaphoresSubmitted::kYes,
        direct_context->flush(write_access->surface(), flush_info, nullptr));

    write_access->ApplyBackendSurfaceEndState();
    direct_context->submit(GrSyncCpu::kYes);
  }

  // Create a backing, fill |draw_area| with |draw_color|, and schedule the
  // overlay
  void ScheduleImageWithOneDraw(gpu::SharedImageUsageSet usage,
                                viz::SharedImageFormat format,
                                const gfx::ColorSpace& color_space,
                                bool has_alpha,
                                const gfx::Rect& draw_area,
                                SkColor draw_color) {
    Mailbox mailbox = Mailbox::Generate();
    std::unique_ptr<SharedImageBacking> backing =
        shared_image_factory_->CreateSharedImage(
            mailbox, format, nullptr, window_size_, color_space,
            kTopLeft_GrSurfaceOrigin,
            has_alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType,
            SharedImageUsageSet(usage), "TestLabel", false);
    ASSERT_NE(nullptr, backing);
    std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
        shared_image_manager_.Register(std::move(backing),
                                       memory_type_tracker_.get());

    std::unique_ptr<SkiaImageRepresentation> skia_representation =
        shared_image_representation_factory_->ProduceSkia(mailbox,
                                                          context_state_);
    FillAreaAndSubmit(skia_representation.get(), draw_area, draw_color);
    skia_representation->SetCleared();

    {
      std::unique_ptr<OverlayImageRepresentation> overlay_representation =
          shared_image_representation_factory_->ProduceOverlay(mailbox);
      ASSERT_NE(nullptr, overlay_representation);
      std::unique_ptr<OverlayImageRepresentation::ScopedReadAccess>
          read_access = overlay_representation->BeginScopedReadAccess();
      ASSERT_NE(nullptr, read_access);
      Microsoft::WRL::ComPtr<IUnknown> layer_content =
          read_access->GetDCLayerOverlayImage()->dcomp_visual_content();
      InitializeVisualTreeWithContent(layer_content.Get());
      CommitAndWait();
    }
  }

  // Runs a sanity check test that verifies backings with different color spaces
  // are valid and contain the values we expect.
  void RunFormatAndColorSpaceTest(gpu::SharedImageUsageSet usage,
                                  viz::SharedImageFormat format,
                                  gfx::ColorSpace color_space,
                                  bool has_alpha,
                                  SkColor clear_color,
                                  SkColor expected_window_color) {
    // Bake the SDR white level into the color space so SharedImage backings can
    // know about it.
    if (color_space.IsAffectedBySDRWhiteLevel()) {
      auto sk_color_space =
          color_space.ToSkColorSpace(gfx::ColorSpace::kDefaultSDRWhiteLevel);
      color_space = gfx::ColorSpace(*sk_color_space, /*is_hdr=*/true);
    }

    DVLOG(2) << "usage = " << CreateLabelForSharedImageUsage(usage)
             << ", format = " << format.ToString()
             << ", color_space = " << color_space.ToString()
             << ", has_alpha = " << has_alpha;

    if (has_alpha) {
      set_background_fill_override(SkColors::kBlack);
    }
    ScheduleImageWithOneDraw(usage, format, color_space, has_alpha,
                             gfx::Rect(window_size()), clear_color);

    // CheckColors accounts for color space conversion shift
    gfx::test::CheckColors(
        expected_window_color,
        gl::GLTestHelper::ReadBackWindowPixel(
            window(),
            gfx::Point(window_size_.width() / 4, window_size_.height() / 4)));
  }

  // Check that a backing whose first draw does not cover the entire surface is
  // initialized internally and the draw does not fail. It is invalid to read
  // from an uninitialized portion of a SharedImage. Incomplete draws still can
  // happen in valid scenarios, however. E.g. if a client over-allocates the
  // backing, but only reads from the part it draws to.
  void RunIncompleteFirstDrawTest(gpu::SharedImageUsageSet usage) {
    // First draw does not cover full surface
    const SkColor expected_color = SK_ColorGREEN;
    ScheduleImageWithOneDraw(usage, viz::SinglePlaneFormat::kRGBA_8888,
                             gfx::ColorSpace::CreateSRGB(), true,
                             gfx::Rect(10, 10), expected_color);

    SkBitmap window_readback =
        gl::GLTestHelper::ReadBackWindow(window(), window_size());

    // The part that we drew should be our expected color
    EXPECT_SKCOLOR_EQ(expected_color, gl::GLTestHelper::GetColorAtPoint(
                                          window_readback, gfx::Point(0, 0)));

#if DCHECK_IS_ON()
    // In DCHECK, DCompImageBackingFactory images are cleared to blue.
    const SkColor expected_window_color = SK_ColorBLUE;
#else
    // In non-DCHECK, the backings are cleared to transparent.
    set_background_fill_override(SkColors::kRed);
    const SkColor expected_window_color = SK_ColorRED;
#endif
    EXPECT_SKCOLOR_EQ(
        expected_window_color,
        gl::GLTestHelper::GetColorAtPoint(window_readback, gfx::Point(0, 10)));
    EXPECT_SKCOLOR_EQ(
        expected_window_color,
        gl::GLTestHelper::GetColorAtPoint(window_readback, gfx::Point(10, 0)));
    EXPECT_SKCOLOR_EQ(
        expected_window_color,
        gl::GLTestHelper::GetColorAtPoint(window_readback, gfx::Point(10, 10)));
    EXPECT_SKCOLOR_EQ(
        expected_window_color,
        gl::GLTestHelper::GetColorAtPoint(window_readback, gfx::Point(99, 99)));
  }

  const gfx::Size& window_size() const { return window_size_; }

 private:
  class TestPlatformDelegate : public ui::PlatformWindowDelegate {
   public:
    // ui::PlatformWindowDelegate implementation.
    void OnBoundsChanged(const BoundsChange& change) override {}
    void OnDamageRect(const gfx::Rect& damaged_region) override {}
    void DispatchEvent(ui::Event* event) override {}
    void OnCloseRequest() override {}
    void OnClosed() override {}
    void OnWindowStateChanged(ui::PlatformWindowState old_state,
                              ui::PlatformWindowState new_state) override {}
    void OnLostCapture() override {}
    void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {}
    void OnWillDestroyAcceleratedWidget() override {}
    void OnAcceleratedWidgetDestroyed() override {}
    void OnActivationChanged(bool active) override {}
    void OnMouseEnter() override {}
  };

  gfx::Size window_size_;
  std::optional<SkColor4f> background_fill_override_;

  TestPlatformDelegate platform_delegate_;
  ui::WinWindow window_;
  gl::ChildWindowWin child_window_;

  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device_;
  Microsoft::WRL::ComPtr<IDCompositionTarget> dcomp_target_;
  Microsoft::WRL::ComPtr<IDCompositionVisual2> dcomp_root_visual_;
};

TEST_F(DCompImageBackingFactoryVisualTreeTest, DCompSurfaceCanDisplay) {
  viz::SharedImageFormat formats[4] = {
      viz::SinglePlaneFormat::kRGBA_8888,
      viz::SinglePlaneFormat::kBGRA_8888,
      viz::SinglePlaneFormat::kRGBX_8888,
      viz::SinglePlaneFormat::kBGRX_8888,
  };

  SkColor test_color = SkColorSetRGB(0x20, 0x40, 0x80);
  SkColor expected_color = test_color;
  for (auto format : formats) {
    RunFormatAndColorSpaceTest(kDCompSurfaceUsage, format,
                               gfx::ColorSpace::CreateSRGB(), false, test_color,
                               expected_color);
  }
}

TEST_F(DCompImageBackingFactoryVisualTreeTest, DCompSurfaceCanDisplayLinear) {
  SkColor test_color = SkColorSetRGB(0x20, 0x40, 0x80);
  SkColor expected_color = SkColorSetRGB(0x35, 0x65, 0xc3);
  RunFormatAndColorSpaceTest(kDCompSurfaceUsage,
                             viz::SinglePlaneFormat::kRGBA_F16,
                             gfx::ColorSpace::CreateSCRGBLinear80Nits(), false,
                             test_color, expected_color);
}

TEST_F(DCompImageBackingFactoryVisualTreeTest,
       DCompSurfaceCanDisplayWithAlpha) {
  viz::SharedImageFormat formats[2] = {
      viz::SinglePlaneFormat::kRGBA_8888,
      viz::SinglePlaneFormat::kBGRA_8888,
  };

  SkColor test_color = SkColorSetARGB(0x80, 0x20, 0x40, 0x80);
  SkColor expected_color = SkColorSetRGB(0x10, 0x20, 0x40);
  for (auto format : formats) {
    RunFormatAndColorSpaceTest(kDCompSurfaceUsage, format,
                               gfx::ColorSpace::CreateSRGB(), true, test_color,
                               expected_color);
  }
}

TEST_F(DCompImageBackingFactoryVisualTreeTest,
       DCompSurfaceCanDisplayLinearWithAlpha) {
  SkColor test_color = SkColorSetARGB(0x80, 0x20, 0x40, 0x80);
  SkColor expected_color = SkColorSetRGB(0x10, 0x20, 0x40);
  RunFormatAndColorSpaceTest(kDCompSurfaceUsage,
                             viz::SinglePlaneFormat::kRGBA_F16,
                             gfx::ColorSpace::CreateSCRGBLinear80Nits(), true,
                             test_color, expected_color);
}

TEST_F(DCompImageBackingFactoryVisualTreeTest, DXGISwapChainCanDisplay) {
  viz::SharedImageFormat formats[4] = {
      viz::SinglePlaneFormat::kRGBA_8888,
      viz::SinglePlaneFormat::kBGRA_8888,
      viz::SinglePlaneFormat::kRGBX_8888,
      viz::SinglePlaneFormat::kBGRX_8888,
  };

  SkColor test_color = SkColorSetRGB(0x20, 0x40, 0x80);
  SkColor expected_color = test_color;
  for (auto format : formats) {
    RunFormatAndColorSpaceTest(kDXGISwapChainUsage, format,
                               gfx::ColorSpace::CreateSRGB(), false, test_color,
                               expected_color);
  }
}

TEST_F(DCompImageBackingFactoryVisualTreeTest, DXGISwapChainCanDisplayLinear) {
  SkColor test_color = SkColorSetRGB(0x20, 0x40, 0x80);
  SkColor expected_color = SkColorSetRGB(0x35, 0x65, 0xc3);
  RunFormatAndColorSpaceTest(kDXGISwapChainUsage,
                             viz::SinglePlaneFormat::kRGBA_F16,
                             gfx::ColorSpace::CreateSCRGBLinear80Nits(), false,
                             test_color, expected_color);
}

TEST_F(DCompImageBackingFactoryVisualTreeTest, DXGISwapChainCanDisplayHDR10) {
  SkColor test_color = SkColorSetRGB(0x20, 0x40, 0x80);
  SkColor expected_color = SkColorSetRGB(0x35, 0x65, 0xc3);
  RunFormatAndColorSpaceTest(
      kDXGISwapChainUsage, viz::SinglePlaneFormat::kRGBA_1010102,
      gfx::ColorSpace::CreateHDR10(), false, test_color, expected_color);
}

TEST_F(DCompImageBackingFactoryVisualTreeTest,
       DXGISwapChainCanDisplayWithAlpha) {
  viz::SharedImageFormat formats[2] = {
      viz::SinglePlaneFormat::kRGBA_8888,
      viz::SinglePlaneFormat::kBGRA_8888,
  };

  SkColor test_color = SkColorSetARGB(0x80, 0x20, 0x40, 0x80);
  SkColor expected_color = SkColorSetRGB(0x10, 0x20, 0x40);
  for (auto format : formats) {
    RunFormatAndColorSpaceTest(kDXGISwapChainUsage, format,
                               gfx::ColorSpace::CreateSRGB(), true, test_color,
                               expected_color);
  }
}

TEST_F(DCompImageBackingFactoryVisualTreeTest,
       DXGISwapChainCanDisplayLinearWithAlpha) {
  SkColor test_color = SkColorSetARGB(0x80, 0x20, 0x40, 0x80);
  SkColor expected_color = SkColorSetRGB(0x10, 0x20, 0x40);
  RunFormatAndColorSpaceTest(kDXGISwapChainUsage,
                             viz::SinglePlaneFormat::kRGBA_F16,
                             gfx::ColorSpace::CreateSCRGBLinear80Nits(), true,
                             test_color, expected_color);
}

TEST_F(DCompImageBackingFactoryVisualTreeTest,
       DXGISwapChainCanDisplayHDR10WithAlpha) {
  SkColor test_color = SkColorSetARGB(0x80, 0x20, 0x40, 0x80);
  SkColor expected_color = SkColorSetRGB(0x10, 0x20, 0x40);
  RunFormatAndColorSpaceTest(
      kDXGISwapChainUsage, viz::SinglePlaneFormat::kRGBA_1010102,
      gfx::ColorSpace::CreateHDR10(), true, test_color, expected_color);
}

TEST_F(DCompImageBackingFactoryVisualTreeTest,
       DXGISwapChainBackingCanDrawMultipleTimes) {
  Mailbox mailbox = Mailbox::Generate();
  std::unique_ptr<SharedImageBacking> backing =
      shared_image_factory_->CreateSharedImage(
          mailbox, viz::SinglePlaneFormat::kRGBA_8888, nullptr,
          gfx::Size(100, 100), gfx::ColorSpace::CreateSRGB(),
          kTopLeft_GrSurfaceOrigin, kOpaque_SkAlphaType, kDXGISwapChainUsage,
          "TestLabel", false);
  ASSERT_NE(nullptr, backing);
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  gfx::Point middle_of_left_half(window_size().width() / 4,
                                 window_size().height() / 2);
  gfx::Point middle_of_right_half(window_size().width() * 3 / 4,
                                  window_size().height() / 2);

  std::unique_ptr<SkiaImageRepresentation> skia_representation =
      shared_image_representation_factory_->ProduceSkia(mailbox,
                                                        context_state_);
  // First draw will clear the entire window
  {
    FillAreaAndSubmit(skia_representation.get(), gfx::Rect(window_size()),
                      SK_ColorRED);
    skia_representation->SetCleared();

    {
      std::unique_ptr<OverlayImageRepresentation> overlay_representation =
          shared_image_representation_factory_->ProduceOverlay(mailbox);
      ASSERT_NE(nullptr, overlay_representation);
      std::unique_ptr<OverlayImageRepresentation::ScopedReadAccess>
          read_access = overlay_representation->BeginScopedReadAccess();
      ASSERT_NE(nullptr, read_access);
      Microsoft::WRL::ComPtr<IUnknown> layer_content =
          read_access->GetDCLayerOverlayImage()->dcomp_visual_content();
      InitializeVisualTreeWithContent(layer_content.Get());
      CommitAndWait();
    }

    SkBitmap window_readback =
        gl::GLTestHelper::ReadBackWindow(window(), window_size());
    EXPECT_SKCOLOR_EQ(SK_ColorRED, gl::GLTestHelper::GetColorAtPoint(
                                       window_readback, middle_of_left_half));
    EXPECT_SKCOLOR_EQ(SK_ColorRED, gl::GLTestHelper::GetColorAtPoint(
                                       window_readback, middle_of_right_half));
  }

  // Next two draws will be partial, drawing different colors to each side
  {
    gfx::Vector2d update_rect_half_size(10, 10);

    FillAreaAndSubmit(
        skia_representation.get(),
        gfx::BoundingRect(middle_of_left_half - update_rect_half_size,
                          middle_of_left_half + update_rect_half_size),
        SK_ColorGREEN);
    FillAreaAndSubmit(
        skia_representation.get(),
        gfx::BoundingRect(middle_of_right_half - update_rect_half_size,
                          middle_of_right_half + update_rect_half_size),
        SK_ColorBLUE);

    // Force a Present on the multiple draws.
    std::ignore = shared_image_representation_factory_->ProduceOverlay(mailbox)
                      ->BeginScopedReadAccess();

    // No Commit is needed, but we should wait for swap chains to flip.
    CommitAndWait();

    SkBitmap window_readback =
        gl::GLTestHelper::ReadBackWindow(window(), window_size());
    EXPECT_SKCOLOR_EQ(SK_ColorGREEN, gl::GLTestHelper::GetColorAtPoint(
                                         window_readback, middle_of_left_half));
    EXPECT_SKCOLOR_EQ(SK_ColorBLUE, gl::GLTestHelper::GetColorAtPoint(
                                        window_readback, middle_of_right_half));
  }
}

TEST_F(DCompImageBackingFactoryVisualTreeTest,
       DCompSurfaceIncompleteFirstDrawInitializesSurface) {
  RunIncompleteFirstDrawTest(kDCompSurfaceUsage);
}

TEST_F(DCompImageBackingFactoryVisualTreeTest,
       DXGISwapChainIncompleteFirstDrawInitializesSurface) {
  RunIncompleteFirstDrawTest(kDXGISwapChainUsage);
}

}  // namespace gpu
