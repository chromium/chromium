// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_test_base.h"

#include <vector>

#include "base/command_line.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image/copy_image_plane.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/Image.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#endif

#if BUILDFLAG(SKIA_USE_METAL)
#include "components/viz/common/gpu/metal_context_provider.h"
#endif

#if BUILDFLAG(SKIA_USE_DAWN) || BUILDFLAG(USE_DAWN)
#include "third_party/dawn/include/dawn/dawn_proc.h"          // nogncheck
#include "third_party/dawn/include/dawn/native/DawnNative.h"  // nogncheck
#endif

namespace {

struct ReadPixelsContext {
  std::unique_ptr<const SkImage::AsyncReadResult> async_result;
  bool finished = false;
};

void OnReadPixelsDone(
    void* raw_ctx,
    std::unique_ptr<const SkImage::AsyncReadResult> async_result) {
  ReadPixelsContext* context = reinterpret_cast<ReadPixelsContext*>(raw_ctx);
  context->async_result = std::move(async_result);
  context->finished = true;
}

void InsertRecordingAndSubmit(gpu::SharedContextState* context,
                              bool sync_cpu = false) {
  CHECK(context->graphite_context());
  auto recording = context->gpu_main_graphite_recorder()->snap();
  if (recording) {
    skgpu::graphite::InsertRecordingInfo info = {};
    info.fRecording = recording.get();
    context->graphite_context()->insertRecording(info);
  }
  context->graphite_context()->submit(sync_cpu
                                          ? skgpu::graphite::SyncToCpu::kYes
                                          : skgpu::graphite::SyncToCpu::kNo);
}

}  // namespace

namespace gpu {

// static
SkBitmap SharedImageTestBase::MakeRedBitmap(SkColorType color_type,
                                            const gfx::Size& size,
                                            size_t added_stride) {
  SkImageInfo info = SkImageInfo::Make(size.width(), size.height(), color_type,
                                       kOpaque_SkAlphaType);
  const size_t stride =
      info.minRowBytes() + added_stride * info.bytesPerPixel();

  SkBitmap bitmap;
  bitmap.allocPixels(info, stride);
  bitmap.eraseColor(SK_ColorRED);
  return bitmap;
}

// static
std::vector<SkBitmap> SharedImageTestBase::AllocateRedBitmaps(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    size_t added_stride) {
  int num_planes = format.NumberOfPlanes();
  std::vector<SkBitmap> bitmaps(num_planes);

  for (int plane = 0; plane < num_planes; ++plane) {
    SkColorType color_type = ToClosestSkColorType(true, format, plane);
    gfx::Size plane_size = format.GetPlaneSize(plane, size);
    bitmaps[plane] = MakeRedBitmap(color_type, plane_size, added_stride);
  }
  return bitmaps;
}

// static
std::vector<SkPixmap> SharedImageTestBase::GetSkPixmaps(
    const std::vector<SkBitmap>& bitmaps) {
  std::vector<SkPixmap> pixmaps;
  for (auto& bitmap : bitmaps) {
    pixmaps.push_back(bitmap.pixmap());
  }
  return pixmaps;
}

SharedImageTestBase::SharedImageTestBase() {
  gpu_preferences_.use_passthrough_cmd_decoder =
      gles2::UsePassthroughCommandDecoder(
          base::CommandLine::ForCurrentProcess()) &&
      gles2::PassthroughCommandDecoderSupported();
}

SharedImageTestBase::~SharedImageTestBase() {
  if (context_state_) {
    // |context_state_| must be destroyed while current.
    context_state_->MakeCurrent(gl_surface_.get(), /*needs_gl=*/true);
  }
}

bool SharedImageTestBase::use_passthrough() const {
  return gpu_preferences_.use_passthrough_cmd_decoder;
}

GrDirectContext* SharedImageTestBase::gr_context() {
  return context_state_->gr_context();
}

GrContextType SharedImageTestBase::gr_context_type() {
  return context_state_->gr_context_type();
}

bool SharedImageTestBase::IsGraphiteDawnSupported() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  return true;
#elif BUILDFLAG(IS_ANDROID) && BUILDFLAG(SKIA_USE_DAWN)
  // Any Android Q+ devices where we have compiled Graphite/Dawn should work.
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
         base::android::SDK_VERSION_Q;
#else
  return false;
#endif
}

void SharedImageTestBase::InitializeContext(GrContextType context_type) {
  gpu_preferences_.gr_context_type = context_type;
#if BUILDFLAG(SKIA_USE_DAWN) || BUILDFLAG(USE_DAWN)
  dawnProcSetProcs(&dawn::native::GetProcs());
#endif

  if (context_type == GrContextType::kGraphiteDawn) {
#if BUILDFLAG(SKIA_USE_DAWN)
    dawn_context_provider_ = DawnContextProvider::CreateWithBackend(
        GetDawnBackendType(), DawnForceFallbackAdapter(), gpu_preferences_,
        DawnContextProvider::DefaultValidateAdapterFn);
    ASSERT_TRUE(dawn_context_provider_);
#else
    FAIL() << "Graphite-Dawn not available";
#endif  // BUILDFLAG(SKIA_USE_DAWN)
  } else if (context_type == GrContextType::kGraphiteMetal) {
#if BUILDFLAG(SKIA_USE_METAL)
    metal_context_provider_ = viz::MetalContextProvider::Create();
    ASSERT_TRUE(metal_context_provider_);
#else
    FAIL() << "Graphite-Metal not available";
#endif  // BUILDFLAG(SKIA_USE_METAL)
  } else if (context_type == GrContextType::kVulkan) {
#if BUILDFLAG(ENABLE_VULKAN)
    vulkan_implementation_ = gpu::CreateVulkanImplementation();
    ASSERT_TRUE(vulkan_implementation_);
    ASSERT_TRUE(vulkan_implementation_->InitializeVulkanInstance());
    vulkan_context_provider_ = viz::VulkanInProcessContextProvider::Create(
        vulkan_implementation_.get());
    ASSERT_TRUE(vulkan_context_provider_);
#else
    FAIL() << "Vulkan not available";
#endif  // BUILDFLAG(ENABLE_VULKAN)
  }

  // Set up a GL context. Even if the GrContext is Vulkan it's still needed.
  gl_surface_ = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplayEGL(),
                                                   gfx::Size());
  ASSERT_TRUE(gl_surface_);
  gl_context_ = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                          gl::GLContextAttribs());
  ASSERT_TRUE(gl_context_);
  bool make_current_result = gl_context_->MakeCurrent(gl_surface_.get());
  ASSERT_TRUE(make_current_result);

  context_state_ = base::MakeRefCounted<SharedContextState>(
      base::MakeRefCounted<gl::GLShareGroup>(), gl_surface_, gl_context_,
      /*use_virtualized_gl_contexts=*/false, base::DoNothing(), context_type
#if BUILDFLAG(ENABLE_VULKAN)
      ,
      vulkan_context_provider_.get()
#else
      ,
      /*vulkan_context_provider=*/nullptr
#endif  // BUILDFLAG(ENABLE_VULKAN)
#if BUILDFLAG(SKIA_USE_DAWN)
          ,
      /*metal_context_provider=*/nullptr, dawn_context_provider_.get()
#elif BUILDFLAG(SKIA_USE_METAL)
      ,
      metal_context_provider_.get()
#endif  // BUILDFLAG(SKIA_USE_DAWN)
  );

  bool initialize_gl = context_state_->InitializeGL(
      gpu_preferences_, base::MakeRefCounted<gles2::FeatureInfo>(
                            gpu_workarounds_, GpuFeatureInfo()));
  ASSERT_TRUE(initialize_gl);

  bool initialize_skia =
      context_state_->InitializeSkia(gpu_preferences_, gpu_workarounds_);
  ASSERT_TRUE(initialize_skia);
}

void SharedImageTestBase::VerifyPixelsWithReadback(
    const Mailbox& mailbox,
    const std::vector<SkBitmap>& expected_bitmaps) {
  if (gr_context()) {
    VerifyPixelsWithReadbackGanesh(mailbox, expected_bitmaps);
  } else {
    CHECK(context_state_->graphite_context());
    VerifyPixelsWithReadbackGraphite(mailbox, expected_bitmaps);
  }
}

void SharedImageTestBase::VerifyPixelsWithReadbackGanesh(
    const Mailbox& mailbox,
    const std::vector<SkBitmap>& expected_bitmaps) {
  // Create Skia representation to readback from.
  auto skia_representation =
      shared_image_representation_factory_.ProduceSkia(mailbox, context_state_);
  ASSERT_TRUE(skia_representation);

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  auto scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  ASSERT_TRUE(scoped_read_access);

  // If this function is used with a backing that produces semaphores or end
  // state then code here needs to be updated to handle them.
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());

  viz::SharedImageFormat format = skia_representation->format();
  gfx::Size size = skia_representation->size();

  int num_planes = format.NumberOfPlanes();
  for (int plane = 0; plane < num_planes; ++plane) {
    SkColorType plane_color_type =
        viz::ToClosestSkColorType(true, format, plane);
    gfx::Size plane_size = format.GetPlaneSize(plane, size);
    SkImageInfo dst_info = SkImageInfo::Make(
        plane_size.width(), plane_size.height(), plane_color_type,
        expected_bitmaps[plane].alphaType());
    SkBitmap dst_bitmap;
    dst_bitmap.allocPixels(dst_info);

    auto* promise_texture = scoped_read_access->promise_image_texture(plane);
    ASSERT_TRUE(promise_texture) << "plane_index=" << plane;

    auto sk_image = SkImages::BorrowTextureFrom(
        gr_context(), promise_texture->backendTexture(),
        skia_representation->surface_origin(), plane_color_type,
        skia_representation->alpha_type(), nullptr);
    ASSERT_TRUE(sk_image) << "plane_index=" << plane;

    ASSERT_TRUE(sk_image->readPixels(dst_info, dst_bitmap.getPixels(),
                                     dst_bitmap.rowBytes(), /*srcX=*/0,
                                     /*srcY=*/0))
        << "plane_index=" << plane;

    EXPECT_TRUE(cc::MatchesBitmap(dst_bitmap, expected_bitmaps[plane],
                                  cc::ExactPixelComparator()))
        << "plane_index=" << plane;
  }
  scoped_read_access->ApplyBackendSurfaceEndState();
}

void SharedImageTestBase::VerifyPixelsWithReadbackGraphite(
    const Mailbox& mailbox,
    const std::vector<SkBitmap>& expected_bitmaps) {
  // Create Skia representation to readback from.
  auto skia_representation =
      shared_image_representation_factory_.ProduceSkia(mailbox, context_state_);
  ASSERT_TRUE(skia_representation);

  auto scoped_read_access = skia_representation->BeginScopedReadAccess(
      /*begin_semaphores=*/{}, /*end_semaphores=*/{});
  ASSERT_TRUE(scoped_read_access);

  viz::SharedImageFormat format = skia_representation->format();
  gfx::Size size = skia_representation->size();

  int num_planes = format.NumberOfPlanes();
  for (int plane = 0; plane < num_planes; ++plane) {
    SkColorType plane_color_type =
        viz::ToClosestSkColorType(true, format, plane);
    gfx::Size plane_size = format.GetPlaneSize(plane, size);
    SkImageInfo dst_info = SkImageInfo::Make(
        plane_size.width(), plane_size.height(), plane_color_type,
        expected_bitmaps[plane].alphaType());
    SkBitmap dst_bitmap;
    dst_bitmap.allocPixels(dst_info);

    auto graphite_texture = scoped_read_access->graphite_texture(plane);
    ASSERT_TRUE(graphite_texture.isValid()) << "plane_index=" << plane;

    ASSERT_TRUE(context_state_->gpu_main_graphite_recorder());
    auto sk_image = SkImages::WrapTexture(
        context_state_->gpu_main_graphite_recorder(), graphite_texture,
        plane_color_type, skia_representation->alpha_type(), nullptr);
    ASSERT_TRUE(sk_image) << "plane_index=" << plane;

    ASSERT_TRUE(context_state_->graphite_context());
    ReadPixelsContext context;
    const SkIRect src_rect = dst_info.bounds();
    context_state_->graphite_context()->asyncRescaleAndReadPixels(
        sk_image.get(), dst_info, src_rect, SkImage::RescaleGamma::kSrc,
        SkImage::RescaleMode::kRepeatedLinear, &OnReadPixelsDone, &context);
    InsertRecordingAndSubmit(context_state_.get(), /*sync_cpu=*/true);
    ASSERT_TRUE(context.finished) << "plane_index=" << plane;
    if (context.async_result) {
      CopyImagePlane(
          static_cast<const uint8_t*>(context.async_result->data(0)),
          context.async_result->rowBytes(0),
          static_cast<uint8_t*>(dst_bitmap.getPixels()), dst_bitmap.rowBytes(),
          dst_info.width() * dst_info.bytesPerPixel(), dst_info.height());
      EXPECT_TRUE(cc::MatchesBitmap(dst_bitmap, expected_bitmaps[plane],
                                    cc::ExactPixelComparator()))
          << "plane_index=" << plane;
    }
  }
}

#if BUILDFLAG(SKIA_USE_DAWN)
wgpu::BackendType SharedImageTestBase::GetDawnBackendType() const {
  return DawnContextProvider::GetDefaultBackendType();
}

bool SharedImageTestBase::DawnForceFallbackAdapter() const {
  return DawnContextProvider::DefaultForceFallbackAdapter();
}
#endif

void PrintTo(GrContextType type, std::ostream* os) {
  *os << GrContextTypeToString(type);
}

}  // namespace gpu
