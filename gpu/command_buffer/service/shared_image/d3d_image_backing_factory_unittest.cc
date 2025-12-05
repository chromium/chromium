// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/shared_image/d3d_image_backing_factory.h"

#include <dawn/dawn_proc.h>
#include <dawn/native/D3D12Backend.h>
#include <dawn/native/DawnNative.h>
#include <dawn/webgpu_cpp.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bits.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/win/scoped_handle.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/graphite_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/compound_image_backing.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_copy_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_copy_strategy.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"
#include "ui/gfx/win/d3d_shared_fence.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#define SCOPED_GL_CLEANUP_VAR(api, func, var)            \
  base::ScopedClosureRunner delete_##var(base::BindOnce( \
      [](gl::GLApi* api, GLuint var) { api->gl##func##Fn(var); }, api, var))

#define SCOPED_GL_CLEANUP_PTR(api, func, n, var)                           \
  base::ScopedClosureRunner delete_##var(base::BindOnce(                   \
      [](gl::GLApi* api, GLuint var) { api->gl##func##Fn(n, &var); }, api, \
      var))

namespace gpu {
namespace {

static const char* kVertexShaderSrc =
    "attribute vec2 a_position;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "  gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);\n"
    "  v_texCoord = (a_position + vec2(1.0, 1.0)) * 0.5;\n"
    "}\n";

void FillNV12(uint8_t* data,
              const gfx::Size& size,
              uint8_t y_fill_value,
              uint8_t u_fill_value,
              uint8_t v_fill_value) {
  const size_t kYPlaneSize = size.width() * size.height();
  memset(data, y_fill_value, kYPlaneSize);
  uint8_t* uv_data = data + kYPlaneSize;
  const size_t kUVPlaneSize = kYPlaneSize / 2;
  for (size_t i = 0; i < kUVPlaneSize; i += 2) {
    uv_data[i] = u_fill_value;
    uv_data[i + 1] = v_fill_value;
  }
}

void CheckNV12(const uint8_t* data,
               size_t stride,
               const gfx::Size& size,
               uint8_t y_fill_value,
               uint8_t u_fill_value,
               uint8_t v_fill_value) {
  const size_t kYPlaneSize = stride * size.height();
  const uint8_t* uv_data = data + kYPlaneSize;
  for (int i = 0; i < size.height(); i++) {
    for (int j = 0; j < size.width(); j++) {
      // ASSERT instead of EXPECT to exit on first failure to avoid log spam.
      ASSERT_EQ(*(data + i * stride + j), y_fill_value);
      if (i < size.height() / 2) {
        const uint8_t uv_value = (j % 2 == 0) ? u_fill_value : v_fill_value;
        ASSERT_EQ(*(uv_data + i * stride + j), uv_value);
      }
    }
  }
}

}  // anonymous namespace

class D3DImageBackingFactoryTestBase : public testing::Test {
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

    memory_type_tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
    shared_image_representation_factory_ =
        std::make_unique<SharedImageRepresentationFactory>(
            &shared_image_manager_, nullptr);
    shared_image_factory_ = std::make_unique<D3DImageBackingFactory>(
        gl::QueryD3D11DeviceObjectFromANGLE(),
        shared_image_manager_.dxgi_shared_handle_manager(), GLFormatCaps());
    dawnProcSetProcs(&dawn::native::GetProcs());
  }

  void TearDown() override { dawnProcSetProcs(nullptr); }

 protected:
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  SharedImageManager shared_image_manager_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  std::unique_ptr<D3DImageBackingFactory> shared_image_factory_;
};

class D3DImageBackingFactoryTest
    : public D3DImageBackingFactoryTestBase,
      public testing::WithParamInterface<GrContextType> {
 public:
  void SetUp() override {
    D3DImageBackingFactoryTestBase::SetUp();
    GpuDriverBugWorkarounds workarounds;
    scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();

    auto gr_context_type = GetParam();
    if (gr_context_type == GrContextType::kGraphiteDawn)
    {
      dawn_context_provider_ =
          DawnContextProvider::Create(GpuPreferences(), GpuFeatureInfo());
    }
    context_state_ = base::MakeRefCounted<SharedContextState>(
        std::move(share_group), surface_, context_,
        /*use_virtualized_gl_contexts=*/false, base::DoNothing(),
        gr_context_type, nullptr, nullptr, dawn_context_provider_.get());
    context_state_->InitializeSkia(GpuPreferences(), workarounds);
    auto feature_info =
        base::MakeRefCounted<gles2::FeatureInfo>(workarounds, GpuFeatureInfo());
    context_state_->InitializeGL(GpuPreferences(), std::move(feature_info));
    copy_manager_ = base::MakeRefCounted<SharedImageCopyManager>();
    copy_manager_->AddStrategy(std::make_unique<SharedMemoryCopyStrategy>());
  }

  void TearDown() override {
    // Need to tear down dawn_context_provider_ before base class reset dawn's
    // proc table.
    context_state_ = nullptr;
    dawn_context_provider_ = nullptr;

    D3DImageBackingFactoryTestBase::TearDown();
  }

 protected:
  bool ReadPixels(const sk_sp<SkImage>& sk_image,
                  const SkImageInfo& dst_info,
                  void* dst_pointer,
                  size_t dst_bytes_per_row,
                  int src_x,
                  int src_y) const {
    if (context_state_->gr_context()) {
      return sk_image->readPixels(context_state_->gr_context(), dst_info,
                                  dst_pointer, dst_bytes_per_row, src_x, src_y);
    }
    DCHECK(context_state_->graphite_shared_context());
    DCHECK(context_state_->gpu_main_graphite_recorder());
    return GraphiteReadPixelsSync(context_state_->graphite_shared_context(),
                                  context_state_->gpu_main_graphite_recorder(),
                                  sk_image.get(), dst_info, dst_pointer,
                                  dst_bytes_per_row, 0, 0);
  }

  void CheckSkiaPixels(
      SkiaImageRepresentation::ScopedReadAccess* scoped_read_access,
      const gfx::Size& size,
      const std::vector<uint8_t>& expected_color) const {
    auto sk_image =
          scoped_read_access->CreateSkImage(context_state_.get());

    EXPECT_EQ(size.width(), sk_image->width());
    EXPECT_EQ(size.height(), sk_image->height());

    const SkImageInfo dst_info =
        SkImageInfo::Make(size.width(), size.height(), kRGBA_8888_SkColorType,
                          kOpaque_SkAlphaType, nullptr);

    const int num_pixels = size.width() * size.height();
    std::vector<uint8_t> dst_pixels(num_pixels * 4);

    // Read back pixels from Sk Image.
    EXPECT_TRUE(ReadPixels(sk_image, dst_info, dst_pixels.data(),
                           dst_info.minRowBytes(), 0, 0));

    for (int i = 0; i < num_pixels; i++) {
      // Compare the pixel values.
      const uint8_t* pixel = dst_pixels.data() + (i * 4);
      EXPECT_EQ(pixel[0], expected_color[0]);
      EXPECT_EQ(pixel[1], expected_color[1]);
      EXPECT_EQ(pixel[2], expected_color[2]);
      EXPECT_EQ(pixel[3], expected_color[3]);
    }
  }

  void CheckSkiaPixels(const Mailbox& mailbox,
                       const gfx::Size& size,
                       const std::vector<uint8_t>& expected_color) const {
    auto skia_representation =
        shared_image_representation_factory_->ProduceSkia(mailbox,
                                                          context_state_);
    ASSERT_NE(skia_representation, nullptr);

    std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
        scoped_read_access =
            skia_representation->BeginScopedReadAccess(nullptr, nullptr);
    EXPECT_TRUE(scoped_read_access);

    CheckSkiaPixels(scoped_read_access.get(), size, expected_color);
  }

  void CheckDawnPixels(
      DawnImageRepresentation::ScopedAccess* scoped_read_access,
      const wgpu::Instance& instance,
      const wgpu::Device& device,
      const gfx::Size& size,
      const std::vector<uint8_t>& expected_color) const;

  // Helper for opening multiple Dawn and Skia scoped access on given mailbox,
  // and checking for the expected color using both APIs concurrently.
  void DawnConcurrentReadTestHelper(const Mailbox& mailbox,
                                    const wgpu::Instance& instance,
                                    const wgpu::Device& device,
                                    const gfx::Size& size,
                                    const std::vector<uint8_t>& expected_color);

  std::unique_ptr<SharedImageRepresentationFactoryRef> CreateVideoImage(
      const gfx::Size& size,
      uint8_t y_fill_value,
      uint8_t u_fill_value,
      uint8_t v_fill_value,
      bool use_shared_handle,
      bool use_factory);
  void RunVideoTest(bool use_shared_handle, bool use_factory);
  void RunOverlayTest(bool use_shared_handle, bool use_factory);
  void RunCreateSharedImageFromHandleTest(DXGI_FORMAT dxgi_format);
  void RunCreateFromSharedMemoryMultiplanarTest(bool use_async_copy);
  void RunMultiplanarUploadAndReadback(bool use_upload_subresource);

  static constexpr wgpu::FeatureName kRequiredFeatures[] = {
      // We need to request internal usage to be able to do operations with
      // internal methods that would need specific usages.
      wgpu::FeatureName::DawnInternalUsages,

      // Required for Dawn interop.
      wgpu::FeatureName::SharedTextureMemoryDXGISharedHandle,
      wgpu::FeatureName::SharedFenceDXGISharedHandle,
  };

  std::unique_ptr<DawnContextProvider> dawn_context_provider_;
  scoped_refptr<SharedContextState> context_state_;
  scoped_refptr<SharedImageCopyManager> copy_manager_;
};

// Test to check interaction between Gl and skia GL representations.
// We write to a GL texture using gl representation and then read from skia
// representation.
TEST_P(D3DImageBackingFactoryTest, GL_SkiaGL) {
  // Create a backing using mailbox.
  auto mailbox = Mailbox::Generate();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const gpu::SharedImageUsageSet usage =
      SHARED_IMAGE_USAGE_GLES2_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage, "TestLabel",
      /*is_thread_safe=*/false);
  ASSERT_NE(backing, nullptr);

  GLenum expected_target = GL_TEXTURE_2D;
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Create a GLTextureImageRepresentation.
  auto gl_representation =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
  EXPECT_EQ(expected_target,
            gl_representation->GetTexturePassthrough()->target());

  std::unique_ptr<GLTexturePassthroughImageRepresentation::ScopedAccess>
      scoped_access = gl_representation->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_TRUE(scoped_access);

  // Create an FBO.
  GLuint fbo = 0;
  gl::GLApi* api = gl::g_current_gl_context;
  api->glGenFramebuffersEXTFn(1, &fbo);
  api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo);

  // Attach the texture to FBO.
  api->glFramebufferTexture2DEXTFn(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      gl_representation->GetTexturePassthrough()->target(),
      gl_representation->GetTexturePassthrough()->service_id(), 0);

  // Set the clear color to green.
  api->glClearColorFn(0.0f, 1.0f, 0.0f, 1.0f);
  api->glClearFn(GL_COLOR_BUFFER_BIT);
  gl_representation->SetCleared();

  scoped_access.reset();
  gl_representation.reset();

  CheckSkiaPixels(mailbox, size, {0, 255, 0, 255});

  factory_ref.reset();
}

// Test to check interaction between Dawn and skia GL representations.
TEST_P(D3DImageBackingFactoryTest, Dawn_SkiaGL) {
  // Find a Dawn D3D12 adapter
  dawn::native::Instance instance;
  wgpu::RequestAdapterOptions adapter_options;
  adapter_options.backendType = wgpu::BackendType::D3D12;
  std::vector<dawn::native::Adapter> adapters =
      instance.EnumerateAdapters(&adapter_options);
  ASSERT_GT(adapters.size(), 0u);

  wgpu::DeviceDescriptor device_descriptor;
  device_descriptor.requiredFeatureCount = std::size(kRequiredFeatures);
  device_descriptor.requiredFeatures = kRequiredFeatures;

  wgpu::Device device =
      wgpu::Device::Acquire(adapters[0].CreateDevice(&device_descriptor));

  // Create a backing using mailbox.
  const auto mailbox = Mailbox::Generate();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  const gpu::SharedImageUsageSet usage =
      SHARED_IMAGE_USAGE_WEBGPU_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage, "TestLabel",
      /*is_thread_safe=*/false);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Clear the shared image to green using Dawn.
  {
    // Create a DawnImageRepresentation.
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(
            mailbox, device, wgpu::BackendType::D3D12, {}, context_state_);
    ASSERT_TRUE(dawn_representation);

    auto scoped_access = dawn_representation->BeginScopedAccess(
        wgpu::TextureUsage::RenderAttachment,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(scoped_access);

    wgpu::Texture texture(scoped_access->texture());

    wgpu::RenderPassColorAttachment color_desc;
    color_desc.view = texture.CreateView();
    color_desc.resolveTarget = nullptr;
    color_desc.loadOp = wgpu::LoadOp::Clear;
    color_desc.storeOp = wgpu::StoreOp::Store;
    color_desc.clearValue = {0, 255, 0, 255};

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &color_desc;
    renderPassDesc.depthStencilAttachment = nullptr;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);
  }

  CheckSkiaPixels(mailbox, size, {0, 255, 0, 255});
  factory_ref.reset();
}

void D3DImageBackingFactoryTest::CheckDawnPixels(
    DawnImageRepresentation::ScopedAccess* scoped_read_access,
    const wgpu::Instance& instance,
    const wgpu::Device& device,
    const gfx::Size& size,
    const std::vector<uint8_t>& expected_color) const {
  wgpu::Texture texture(scoped_read_access->texture());

  uint32_t buffer_stride = static_cast<uint32_t>(
      base::bits::AlignUpDeprecatedDoNotUse(size.width() * 4, 256));
  size_t buffer_size = static_cast<size_t>(size.height()) * buffer_stride;
  wgpu::BufferDescriptor buffer_desc{
      .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
      .size = buffer_size};
  wgpu::Buffer buffer = device.CreateBuffer(&buffer_desc);

  wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
  auto src =
      wgpu::TexelCopyTextureInfo{.texture = texture, .origin = {0, 0, 0}};
  auto dst = wgpu::TexelCopyBufferInfo{.layout = {.bytesPerRow = buffer_stride},
                                       .buffer = buffer};
  auto copy_size = wgpu::Extent3D{static_cast<uint32_t>(size.width()),
                                  static_cast<uint32_t>(size.height()), 1};
  encoder.CopyTextureToBuffer(&src, &dst, &copy_size);
  wgpu::CommandBuffer commands = encoder.Finish();

  wgpu::Queue queue = device.GetQueue();
  queue.Submit(1, &commands);

  wgpu::FutureWaitInfo wait_info{buffer.MapAsync(
      wgpu::MapMode::Read, 0, buffer_desc.size, wgpu::CallbackMode::WaitAnyOnly,
      [&](wgpu::MapAsyncStatus status, wgpu::StringView) {
        ASSERT_EQ(status, wgpu::MapAsyncStatus::Success);
      })};
  wgpu::WaitStatus status =
      instance.WaitAny(1, &wait_info, std::numeric_limits<uint64_t>::max());
  DCHECK(status == wgpu::WaitStatus::Success);

  const uint8_t* dst_pixels =
      reinterpret_cast<const uint8_t*>(buffer.GetConstMappedRange());
  for (int row = 0; row < size.height(); row++) {
    for (int col = 0; col < size.width(); col++) {
      // Compare the pixel values.
      const uint8_t* pixel = dst_pixels + (row * buffer_stride) + col * 4;
      EXPECT_EQ(pixel[0], expected_color[0]);
      EXPECT_EQ(pixel[1], expected_color[1]);
      EXPECT_EQ(pixel[2], expected_color[2]);
      EXPECT_EQ(pixel[3], expected_color[3]);
    }
  }
}

// Opens two Skia and two Dawn ScopedReadAccess and performs concurrent reads
// using both APIs.
void D3DImageBackingFactoryTest::DawnConcurrentReadTestHelper(
    const Mailbox& mailbox,
    const wgpu::Instance& instance,
    const wgpu::Device& device,
    const gfx::Size& size,
    const std::vector<uint8_t>& expected_color) {
  auto dawn_representation1 = shared_image_representation_factory_->ProduceDawn(
      mailbox, device, wgpu::BackendType::D3D12, {}, context_state_);
  ASSERT_TRUE(dawn_representation1);

  auto dawn_access1 = dawn_representation1->BeginScopedAccess(
      wgpu::TextureUsage::CopySrc,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  ASSERT_TRUE(dawn_access1);

  auto dawn_representation2 = shared_image_representation_factory_->ProduceDawn(
      mailbox, device, wgpu::BackendType::D3D12, {}, context_state_);
  ASSERT_TRUE(dawn_representation2);

  auto dawn_access2 = dawn_representation2->BeginScopedAccess(
      wgpu::TextureUsage::CopySrc,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  ASSERT_TRUE(dawn_access2);

  auto skia_representation1 = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
  ASSERT_NE(skia_representation1, nullptr);

  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> skia_access1 =
      skia_representation1->BeginScopedReadAccess(nullptr, nullptr);
  EXPECT_TRUE(skia_access1);

  auto skia_representation2 = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
  ASSERT_NE(skia_representation2, nullptr);

  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> skia_access2 =
      skia_representation2->BeginScopedReadAccess(nullptr, nullptr);
  EXPECT_TRUE(skia_access2);

  CheckDawnPixels(dawn_access1.get(), instance, device, size, expected_color);
  CheckDawnPixels(dawn_access2.get(), instance, device, size, expected_color);
  CheckSkiaPixels(skia_access1.get(), size, expected_color);
  CheckSkiaPixels(skia_access2.get(), size, expected_color);
}

TEST_P(D3DImageBackingFactoryTest, Dawn_ConcurrentReads) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      shared_image_factory_->GetDeviceForTesting();
  if (!gfx::D3DSharedFence::IsSupported(d3d11_device.Get())) {
    GTEST_SKIP();
  }

  // Find a Dawn D3D12 adapter
  static constexpr auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
  wgpu::InstanceDescriptor instance_desc = {
      .requiredFeatureCount = 1,
      .requiredFeatures = &kTimedWaitAny,
  };
  dawn::native::Instance instance(&instance_desc);
  wgpu::RequestAdapterOptions adapter_options;
  adapter_options.backendType = wgpu::BackendType::D3D12;
  std::vector<dawn::native::Adapter> adapters =
      instance.EnumerateAdapters(&adapter_options);
  ASSERT_GT(adapters.size(), 0u);

  wgpu::DeviceDescriptor device_descriptor;
  device_descriptor.requiredFeatureCount = std::size(kRequiredFeatures);
  device_descriptor.requiredFeatures = kRequiredFeatures;

  wgpu::Device device =
      wgpu::Device::Acquire(adapters[0].CreateDevice(&device_descriptor));

  // Create a backing using mailbox.
  const auto mailbox = Mailbox::Generate();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  const gpu::SharedImageUsageSet usage =
      SHARED_IMAGE_USAGE_WEBGPU_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE |
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage, "TestLabel",
      /*is_thread_safe=*/false);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Clear to red using Skia.
  {
    auto skia_representation =
        shared_image_representation_factory_->ProduceSkia(mailbox,
                                                          context_state_);
    ASSERT_NE(skia_representation, nullptr);

    auto scoped_write_access = skia_representation->BeginScopedWriteAccess(
        /*begin_semaphores=*/nullptr, /*end_semaphores=*/nullptr,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    EXPECT_TRUE(scoped_write_access);

    SkCanvas* canvas = scoped_write_access->surface()->getCanvas();
    canvas->clear(SkColors::kRed);
    context_state_->FlushWriteAccess(scoped_write_access.get());

    skia_representation->SetCleared();
  }

  // Check if pixels are red using concurrent reads.
  DawnConcurrentReadTestHelper(mailbox, instance.Get(), device, size,
                               {255, 0, 0, 255});

  // Clear the shared image to green using Dawn.
  {
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(
            mailbox, device, wgpu::BackendType::D3D12, {}, context_state_);
    ASSERT_TRUE(dawn_representation);

    auto scoped_access = dawn_representation->BeginScopedAccess(
        wgpu::TextureUsage::RenderAttachment,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    ASSERT_TRUE(scoped_access);

    wgpu::Texture texture(scoped_access->texture());

    wgpu::RenderPassColorAttachment color_desc;
    color_desc.view = texture.CreateView();
    color_desc.resolveTarget = nullptr;
    color_desc.loadOp = wgpu::LoadOp::Clear;
    color_desc.storeOp = wgpu::StoreOp::Store;
    color_desc.clearValue = {0, 255, 0, 255};

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &color_desc;
    renderPassDesc.depthStencilAttachment = nullptr;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);
  }

  // Check if pixels are green using concurrent reads.
  DawnConcurrentReadTestHelper(mailbox, instance.Get(), device, size,
                               {0, 255, 0, 255});
}

// 1. Draw a color to texture through GL
// 2. Do not call SetCleared so we can test Dawn Lazy clear
// 3. Begin render pass in Dawn, but do not do anything
// 4. Verify through CheckSkiaPixel that GL drawn color not seen
TEST_P(D3DImageBackingFactoryTest, GL_Dawn_Skia_UnclearTexture) {
  // Create a backing using mailbox.
  auto mailbox = Mailbox::Generate();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const gpu::SharedImageUsageSet usage = SHARED_IMAGE_USAGE_GLES2_WRITE |
                                         SHARED_IMAGE_USAGE_DISPLAY_READ |
                                         SHARED_IMAGE_USAGE_WEBGPU_WRITE;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage, "TestLabel",
      /*is_thread_safe=*/false);
  ASSERT_NE(backing, nullptr);

  GLenum expected_target = GL_TEXTURE_2D;
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());
  {
    // Create a GLTextureImageRepresentation.
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
            mailbox);
    EXPECT_EQ(expected_target,
              gl_representation->GetTexturePassthrough()->target());

    std::unique_ptr<GLTexturePassthroughImageRepresentation::ScopedAccess>
        gl_scoped_access = gl_representation->BeginScopedAccess(
            GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
            SharedImageRepresentation::AllowUnclearedAccess::kYes);
    EXPECT_TRUE(gl_scoped_access);

    // Create an FBO.
    GLuint fbo = 0;
    gl::GLApi* api = gl::g_current_gl_context;
    api->glGenFramebuffersEXTFn(1, &fbo);
    api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo);

    // Attach the texture to FBO.
    api->glFramebufferTexture2DEXTFn(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        gl_representation->GetTexturePassthrough()->target(),
        gl_representation->GetTexturePassthrough()->service_id(), 0);

    // Set the clear color to green.
    api->glClearColorFn(0.0f, 1.0f, 0.0f, 1.0f);
    api->glClearFn(GL_COLOR_BUFFER_BIT);

    // Don't call SetCleared, we want to see if Dawn will lazy clear the texture
    EXPECT_FALSE(factory_ref->IsCleared());
  }

  // Find a Dawn D3D12 adapter
  dawn::native::Instance instance;
  wgpu::RequestAdapterOptions adapter_options;
  adapter_options.backendType = wgpu::BackendType::D3D12;
  std::vector<dawn::native::Adapter> adapters =
      instance.EnumerateAdapters(&adapter_options);
  ASSERT_GT(adapters.size(), 0u);

  wgpu::DeviceDescriptor device_descriptor;
  device_descriptor.requiredFeatureCount = std::size(kRequiredFeatures);
  device_descriptor.requiredFeatures = kRequiredFeatures;

  wgpu::Device device =
      wgpu::Device::Acquire(adapters[0].CreateDevice(&device_descriptor));

  {
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(
            mailbox, device, wgpu::BackendType::D3D12, {}, context_state_);
    ASSERT_TRUE(dawn_representation);

    auto dawn_scoped_access = dawn_representation->BeginScopedAccess(
        wgpu::TextureUsage::RenderAttachment,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(dawn_scoped_access);

    wgpu::Texture texture(dawn_scoped_access->texture());
    wgpu::RenderPassColorAttachment color_desc;
    color_desc.view = texture.CreateView();
    color_desc.resolveTarget = nullptr;
    color_desc.loadOp = wgpu::LoadOp::Load;
    color_desc.storeOp = wgpu::StoreOp::Store;

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &color_desc;
    renderPassDesc.depthStencilAttachment = nullptr;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);
  }

  // Check skia pixels returns black since texture was lazy cleared in Dawn
  EXPECT_TRUE(factory_ref->IsCleared());
  CheckSkiaPixels(mailbox, size, {0, 0, 0, 0});
  factory_ref.reset();
}

// 1. Draw  a color to texture through Dawn
// 2. Set the renderpass storeOp = Discard
// 3. Texture in Dawn will stay as uninitialized
// 3. Expect skia to fail to access the texture because texture is not
// initialized
TEST_P(D3DImageBackingFactoryTest, UnclearDawn_SkiaFails) {
  // Create a backing using mailbox.
  auto mailbox = Mailbox::Generate();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const gpu::SharedImageUsageSet usage =
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage, "TestLabel",
      /*is_thread_safe=*/false);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Find a Dawn D3D12 adapter
  dawn::native::Instance instance;
  wgpu::RequestAdapterOptions adapter_options;
  adapter_options.backendType = wgpu::BackendType::D3D12;
  std::vector<dawn::native::Adapter> adapters =
      instance.EnumerateAdapters(&adapter_options);
  ASSERT_GT(adapters.size(), 0u);

  wgpu::DeviceDescriptor device_descriptor;
  device_descriptor.requiredFeatureCount = std::size(kRequiredFeatures);
  device_descriptor.requiredFeatures = kRequiredFeatures;

  wgpu::Device device =
      wgpu::Device::Acquire(adapters[0].CreateDevice(&device_descriptor));
  {
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(
            mailbox, device, wgpu::BackendType::D3D12, {}, context_state_);
    ASSERT_TRUE(dawn_representation);

    auto dawn_scoped_access = dawn_representation->BeginScopedAccess(
        wgpu::TextureUsage::RenderAttachment,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(dawn_scoped_access);

    wgpu::Texture texture(dawn_scoped_access->texture());
    wgpu::RenderPassColorAttachment color_desc;
    color_desc.view = texture.CreateView();
    color_desc.resolveTarget = nullptr;
    color_desc.loadOp = wgpu::LoadOp::Clear;
    color_desc.storeOp = wgpu::StoreOp::Discard;
    color_desc.clearValue = {0, 255, 0, 255};

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &color_desc;
    renderPassDesc.depthStencilAttachment = nullptr;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);
  }

  EXPECT_FALSE(factory_ref->IsCleared());

  // Produce skia representation
  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
  ASSERT_NE(skia_representation, nullptr);

  // Expect BeginScopedReadAccess to fail because sharedImage is uninitialized
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
      scoped_read_access =
          skia_representation->BeginScopedReadAccess(nullptr, nullptr);
  EXPECT_EQ(scoped_read_access, nullptr);
}

// Test that Skia trying to access uninitialized SharedImage will fail
TEST_P(D3DImageBackingFactoryTest, SkiaAccessFirstFails) {
  // Create a mailbox.
  auto mailbox = Mailbox::Generate();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const gpu::SharedImageUsageSet usage = SHARED_IMAGE_USAGE_DISPLAY_READ;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage, "TestLabel",
      /*is_thread_safe=*/false);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Produce skia representation
  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
  ASSERT_NE(skia_representation, nullptr);
  EXPECT_FALSE(skia_representation->IsCleared());

  // Expect BeginScopedReadAccess to fail because sharedImage is uninitialized
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
      scoped_read_access =
          skia_representation->BeginScopedReadAccess(nullptr, nullptr);
  EXPECT_EQ(scoped_read_access, nullptr);
}

TEST_P(D3DImageBackingFactoryTest, CreateFromPixelData) {
  auto mailbox = Mailbox::Generate();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const gpu::SharedImageUsageSet usage = SHARED_IMAGE_USAGE_DISPLAY_READ;
  const std::vector<uint8_t> pixel_data = {0x01, 0x02, 0x03, 0x04};
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, size, color_space, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, usage, "TestLabel",
      /*is_thread_safe=*/false, base::span<const uint8_t>(pixel_data));
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  CheckSkiaPixels(mailbox, size, pixel_data);
}

void D3DImageBackingFactoryTest::RunCreateSharedImageFromHandleTest(
    DXGI_FORMAT dxgi_format) {
  auto mailbox = Mailbox::Generate();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  // This function tests concurrent GL reads of two SharedImages created from
  // the underlying handle.
  const gpu::SharedImageUsageSet usage =
      SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_DISPLAY_READ;
  const GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  const SkAlphaType alpha_type = kPremul_SkAlphaType;

  EXPECT_TRUE(shared_image_factory_->CanCreateSharedImage(
      usage, format, size, /*thread_safe=*/false, gfx::DXGI_SHARED_HANDLE,
      GrContextType::kGL, /*pixel_data=*/{}));

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      shared_image_factory_->GetDeviceForTesting();

  D3D11_TEXTURE2D_DESC desc;
  desc.Width = size.width();
  desc.Height = size.height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = dxgi_format;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags =
      D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = d3d11_device->CreateTexture2D(&desc, nullptr, &d3d11_texture);
  ASSERT_EQ(hr, S_OK);

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  hr = d3d11_texture.As(&dxgi_resource);
  ASSERT_EQ(hr, S_OK);

  HANDLE shared_handle;
  hr = dxgi_resource->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &shared_handle);
  ASSERT_EQ(hr, S_OK);

  gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle{
      gfx::DXGIHandle(base::win::ScopedHandle(shared_handle))};

  // Clone before moving the handle in CreateSharedImage.
  auto dup_handle = gpu_memory_buffer_handle.Clone();

  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      "TestLabel", /*is_thread_safe=*/false,
      std::move(gpu_memory_buffer_handle));
  ASSERT_NE(backing, nullptr);

  EXPECT_EQ(backing->format(), format);
  EXPECT_EQ(backing->size(), size);
  EXPECT_EQ(backing->color_space(), color_space);
  EXPECT_EQ(backing->surface_origin(), surface_origin);
  EXPECT_EQ(backing->alpha_type(), alpha_type);
  EXPECT_EQ(backing->mailbox(), mailbox);
  EXPECT_TRUE(backing->IsCleared());

  D3DImageBacking* backing_d3d = static_cast<D3DImageBacking*>(backing.get());
  EXPECT_EQ(
      backing_d3d->dxgi_shared_handle_state_for_testing()->GetSharedHandle(),
      shared_handle);

  // Check that a second backing created from the duplicated handle shares the
  // shared handle state and texture with the first backing.
  auto dup_mailbox = Mailbox::Generate();
  auto dup_backing = shared_image_factory_->CreateSharedImage(
      dup_mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      "TestLabel", /*is_thread_safe=*/false, std::move(dup_handle));
  ASSERT_NE(dup_backing, nullptr);

  EXPECT_EQ(dup_backing->format(), format);
  EXPECT_EQ(dup_backing->size(), size);
  EXPECT_EQ(dup_backing->color_space(), color_space);
  EXPECT_EQ(dup_backing->surface_origin(), surface_origin);
  EXPECT_EQ(dup_backing->alpha_type(), alpha_type);
  EXPECT_EQ(dup_backing->mailbox(), dup_mailbox);
  EXPECT_TRUE(dup_backing->IsCleared());

  D3DImageBacking* dup_backing_d3d =
      static_cast<D3DImageBacking*>(dup_backing.get());
  EXPECT_EQ(dup_backing_d3d->dxgi_shared_handle_state_for_testing(),
            backing_d3d->dxgi_shared_handle_state_for_testing());
  EXPECT_EQ(dup_backing_d3d->d3d11_texture_for_testing(),
            backing_d3d->d3d11_texture_for_testing());

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  std::unique_ptr<SharedImageRepresentationFactoryRef> dup_factory_ref =
      shared_image_manager_.Register(std::move(dup_backing),
                                     memory_type_tracker_.get());

  // Check that concurrent read access using the duplicated handle works.
  auto gl_representation =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
  EXPECT_TRUE(gl_representation);

  std::unique_ptr<GLTexturePassthroughImageRepresentation::ScopedAccess>
      scoped_access = gl_representation->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_TRUE(scoped_access);

  auto dup_gl_representation =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          dup_mailbox);
  EXPECT_TRUE(dup_gl_representation);

  std::unique_ptr<GLTexturePassthroughImageRepresentation::ScopedAccess>
      dup_scoped_access = dup_gl_representation->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_TRUE(dup_scoped_access);
}

TEST_P(D3DImageBackingFactoryTest, CreateSharedImageFromHandleFormatUNORM) {
  RunCreateSharedImageFromHandleTest(DXGI_FORMAT_R8G8B8A8_UNORM);
}

TEST_P(D3DImageBackingFactoryTest, CreateSharedImageFromHandleFormatTYPELESS) {
  RunCreateSharedImageFromHandleTest(DXGI_FORMAT_R8G8B8A8_TYPELESS);
}

// Tests that writing to a Skia representation of a D3DImageBacking created
// from a shared handle is reflected in a second backing created from the
// same handle.
TEST_P(D3DImageBackingFactoryTest, SkiaWriteReadWithSharedHandle) {
  auto mailbox1 = Mailbox::Generate();
  auto mailbox2 = Mailbox::Generate();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const gpu::SharedImageUsageSet usage =
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE;
  const GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  const SkAlphaType alpha_type = kPremul_SkAlphaType;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      shared_image_factory_->GetDeviceForTesting();

  D3D11_TEXTURE2D_DESC desc;
  desc.Width = size.width();
  desc.Height = size.height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags =
      D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = d3d11_device->CreateTexture2D(&desc, nullptr, &d3d11_texture);
  ASSERT_EQ(hr, S_OK);

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  hr = d3d11_texture.As(&dxgi_resource);
  ASSERT_EQ(hr, S_OK);

  HANDLE shared_handle;
  hr = dxgi_resource->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &shared_handle);
  ASSERT_EQ(hr, S_OK);

  gfx::GpuMemoryBufferHandle handle1{
      gfx::DXGIHandle(base::win::ScopedHandle(shared_handle))};
  auto handle2 = handle1.Clone();

  auto backing1 = shared_image_factory_->CreateSharedImage(
      mailbox1, format, size, color_space, surface_origin, alpha_type, usage,
      "TestLabel", /*is_thread_safe=*/false, std::move(handle1));
  ASSERT_TRUE(backing1);
  auto backing2 = shared_image_factory_->CreateSharedImage(
      mailbox2, format, size, color_space, surface_origin, alpha_type, usage,
      "TestLabel", /*is_thread_safe=*/false, std::move(handle2));
  ASSERT_TRUE(backing2);

  D3DImageBacking* backing1_d3d = static_cast<D3DImageBacking*>(backing1.get());
  D3DImageBacking* backing2_d3d = static_cast<D3DImageBacking*>(backing2.get());
  ASSERT_EQ(backing1_d3d->dxgi_shared_handle_state_for_testing(),
            backing2_d3d->dxgi_shared_handle_state_for_testing());

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref1 =
      shared_image_manager_.Register(std::move(backing1),
                                     memory_type_tracker_.get());
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref2 =
      shared_image_manager_.Register(std::move(backing2),
                                     memory_type_tracker_.get());

  // Write blue to the first backing.
  {
    auto skia_representation1 =
        shared_image_representation_factory_->ProduceSkia(mailbox1,
                                                          context_state_);
    ASSERT_TRUE(skia_representation1);
    auto scoped_write_access = skia_representation1->BeginScopedWriteAccess(
        /*begin_semaphores=*/nullptr, /*end_semaphores=*/nullptr,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(scoped_write_access);

    SkCanvas* canvas = scoped_write_access->surface()->getCanvas();
    canvas->clear(SkColors::kBlue);
    context_state_->FlushWriteAccess(scoped_write_access.get());
    context_state_->SubmitIfNecessary({}, true);
    skia_representation1->SetCleared();
  }

  // Read from the second backing and check that it is blue.
  CheckSkiaPixels(mailbox2, size, {0, 0, 255, 255});
}

// Test to check external image stored in the backing can be reused
TEST_P(D3DImageBackingFactoryTest, Dawn_ReuseExternalImage) {
  // Create a backing using mailbox.
  auto mailbox = Mailbox::Generate();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const gpu::SharedImageUsageSet usage =
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage, "TestLabel",
      /*is_thread_safe=*/false);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Find a Dawn D3D12 adapter
  dawn::native::Instance instance;
  wgpu::RequestAdapterOptions adapter_options;
  adapter_options.backendType = wgpu::BackendType::D3D12;
  std::vector<dawn::native::Adapter> adapters =
      instance.EnumerateAdapters(&adapter_options);
  ASSERT_GT(adapters.size(), 0u);

  wgpu::DeviceDescriptor device_descriptor;
  device_descriptor.requiredFeatureCount = std::size(kRequiredFeatures);
  device_descriptor.requiredFeatures = kRequiredFeatures;

  wgpu::Device device =
      wgpu::Device::Acquire(adapters[0].CreateDevice(&device_descriptor));

  const wgpu::TextureUsage texture_usage = wgpu::TextureUsage::RenderAttachment;

  // Create the first Dawn texture then clear it to green.
  {
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(
            mailbox, device, wgpu::BackendType::D3D12, {}, context_state_);
    ASSERT_TRUE(dawn_representation);

    auto scoped_access = dawn_representation->BeginScopedAccess(
        texture_usage, SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(scoped_access);

    wgpu::Texture texture(scoped_access->texture());

    wgpu::RenderPassColorAttachment color_desc;
    color_desc.view = texture.CreateView();
    color_desc.resolveTarget = nullptr;
    color_desc.loadOp = wgpu::LoadOp::Clear;
    color_desc.storeOp = wgpu::StoreOp::Store;
    color_desc.clearValue = {0, 255, 0, 255};

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &color_desc;
    renderPassDesc.depthStencilAttachment = nullptr;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);
  }

  CheckSkiaPixels(mailbox, size, {0, 255, 0, 255});

  // Create another Dawn texture then clear it with another color.
  {
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(
            mailbox, device, wgpu::BackendType::D3D12, {}, context_state_);
    ASSERT_TRUE(dawn_representation);

    // Check again that the texture is still green
    CheckSkiaPixels(mailbox, size, {0, 255, 0, 255});

    auto scoped_access = dawn_representation->BeginScopedAccess(
        texture_usage, SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(scoped_access);

    wgpu::Texture texture(scoped_access->texture());

    wgpu::RenderPassColorAttachment color_desc;
    color_desc.view = texture.CreateView();
    color_desc.resolveTarget = nullptr;
    color_desc.loadOp = wgpu::LoadOp::Clear;
    color_desc.storeOp = wgpu::StoreOp::Store;
    color_desc.clearValue = {255, 0, 0, 255};

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &color_desc;
    renderPassDesc.depthStencilAttachment = nullptr;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);
  }

  CheckSkiaPixels(mailbox, size, {255, 0, 0, 255});

  factory_ref.reset();
}

// Check if making Dawn have the last ref works without a current GL context.
TEST_P(D3DImageBackingFactoryTest, Dawn_HasLastRef) {
  // Create a backing using mailbox.
  auto mailbox = Mailbox::Generate();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const gpu::SharedImageUsageSet usage =
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_WEBGPU_READ;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage, "TestLabel",
      /*is_thread_safe=*/false);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Find a Dawn D3D12 adapter
  dawn::native::Instance instance;
  wgpu::RequestAdapterOptions adapter_options;
  adapter_options.backendType = wgpu::BackendType::D3D12;
  std::vector<dawn::native::Adapter> adapters =
      instance.EnumerateAdapters(&adapter_options);
  ASSERT_GT(adapters.size(), 0u);

  wgpu::DeviceDescriptor device_descriptor;
  device_descriptor.requiredFeatureCount = std::size(kRequiredFeatures);
  device_descriptor.requiredFeatures = kRequiredFeatures;

  wgpu::Device device =
      wgpu::Device::Acquire(adapters[0].CreateDevice(&device_descriptor));
  auto dawn_representation = shared_image_representation_factory_->ProduceDawn(
      mailbox, device, wgpu::BackendType::D3D12, {}, context_state_);
  ASSERT_NE(dawn_representation, nullptr);

  // Creating the Skia representation will also create a temporary GL texture.
  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
  ASSERT_NE(skia_representation, nullptr);

  // Drop Skia representation and factory ref so that the Dawn representation
  // has the last ref.
  skia_representation.reset();
  factory_ref.reset();

  // Ensure no GL context is current.
  context_->ReleaseCurrent(surface_.get());

  // This shouldn't crash due to no GL context being current.
  dawn_representation.reset();

  // Make context current so that it can be destroyed.
  context_->MakeCurrent(surface_.get());
}

std::unique_ptr<SharedImageRepresentationFactoryRef>
D3DImageBackingFactoryTest::CreateVideoImage(const gfx::Size& size,
                                             uint8_t y_fill_value,
                                             uint8_t u_fill_value,
                                             uint8_t v_fill_value,
                                             bool use_shared_handle,
                                             bool use_factory) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      shared_image_factory_->GetDeviceForTesting();

  const size_t kDataSize = size.width() * size.height() * 3 / 2;

  std::vector<uint8_t> video_data(kDataSize);
  FillNV12(video_data.data(), size, y_fill_value, u_fill_value, v_fill_value);

  D3D11_SUBRESOURCE_DATA data = {};
  data.pSysMem = static_cast<const void*>(video_data.data());
  data.SysMemPitch = static_cast<UINT>(size.width());

  CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_NV12, size.width(), size.height(), 1,
                             1, D3D11_BIND_SHADER_RESOURCE);
  if (use_shared_handle) {
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                     D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = d3d11_device->CreateTexture2D(&desc, &data, &d3d11_texture);
  if (FAILED(hr))
    return {};

  // The video tests read from the created SharedImages via GL.
  gpu::SharedImageUsageSet usage =
      gpu::SHARED_IMAGE_USAGE_VIDEO_DECODE |
      gpu::SHARED_IMAGE_USAGE_GLES2_READ | gpu::SHARED_IMAGE_USAGE_RASTER_READ |
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  base::win::ScopedHandle shared_handle;
  if (use_shared_handle) {
    Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
    hr = d3d11_texture.As(&dxgi_resource);
    DCHECK_EQ(hr, S_OK);

    HANDLE handle = nullptr;
    hr = dxgi_resource->CreateSharedHandle(
        nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        nullptr, &handle);
    if (FAILED(hr))
      return {};

    shared_handle.Set(handle);
    DCHECK(shared_handle.is_valid());

    // TODO(dawn:551): Extend the tests using this util to test reading the
    // textures via Dawn.
    usage |= gpu::SHARED_IMAGE_USAGE_WEBGPU_READ;
  }

  const gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  std::unique_ptr<SharedImageBacking> shared_image_backing;
  if (use_factory) {
    gfx::GpuMemoryBufferHandle gmb_handle(
        gfx::DXGIHandle(std::move(shared_handle)));
    DCHECK(gmb_handle.dxgi_handle().IsValid());

    shared_image_backing = shared_image_factory_->CreateSharedImage(
        mailbox, viz::MultiPlaneFormat::kNV12, size, gfx::ColorSpace(),
        kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage, "TestLabel",
        /*is_thread_safe=*/false, std::move(gmb_handle));
    if (!shared_image_backing) {
      return {};
    }
  } else {
    scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state;
    if (use_shared_handle) {
      dxgi_shared_handle_state =
          shared_image_manager_.dxgi_shared_handle_manager()
              ->CreateAnonymousSharedHandleState(std::move(shared_handle),
                                                 d3d11_texture);
    }
    shared_image_backing = D3DImageBacking::Create(
        mailbox, viz::MultiPlaneFormat::kNV12, size, gfx::ColorSpace(),
        kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage, "TestLabel",
        d3d11_texture, std::move(dxgi_shared_handle_state),
        context_state_->GetGLFormatCaps(), GL_TEXTURE_EXTERNAL_OES,
        /*array_slice=*/0, /*plane_index=*/0u);
    // Need to clear the backing created with shared handle.
    shared_image_backing->SetCleared();
  }

  auto& backing = shared_image_backing;
  EXPECT_EQ(backing->mailbox(), mailbox);
  EXPECT_EQ(backing->size(), size);
  EXPECT_EQ(backing->format(), viz::MultiPlaneFormat::kNV12);
  EXPECT_EQ(backing->color_space(), gfx::ColorSpace());
  EXPECT_EQ(backing->surface_origin(), kTopLeft_GrSurfaceOrigin);
  EXPECT_EQ(backing->alpha_type(), kPremul_SkAlphaType);
  EXPECT_EQ(backing->usage(), usage);
  EXPECT_TRUE(backing->IsCleared());

  auto shared_image_ref = shared_image_manager_.Register(
      std::move(backing), memory_type_tracker_.get());

  return shared_image_ref;
}

void D3DImageBackingFactoryTest::RunVideoTest(bool use_shared_handle,
                                              bool use_factory) {
  const gfx::Size size(32, 32);

  const uint8_t kYFillValue = 0x12;
  const uint8_t kUFillValue = 0x23;
  const uint8_t kVFillValue = 0x34;

  auto shared_image_ref =
      CreateVideoImage(size, kYFillValue, kUFillValue, kVFillValue,
                       use_shared_handle, use_factory);
  ASSERT_TRUE(shared_image_ref);

  // Setup GL shaders, framebuffers, uniforms, etc.
  static const char* kVideoFragmentShaderSrcTextureExternal =
      "#extension GL_OES_EGL_image_external : require\n"
      "precision mediump float;\n"
      "uniform samplerExternalOES u_texture_y;\n"
      "uniform samplerExternalOES u_texture_uv;\n"
      "varying vec2 v_texCoord;\n"
      "void main() {\n"
      "  gl_FragColor.r = texture2D(u_texture_y, v_texCoord).r;\n"
      "  gl_FragColor.gb = texture2D(u_texture_uv, v_texCoord).rg;\n"
      "  gl_FragColor.a = 1.0;\n"
      "}\n";
  static const char* kVideoFragmentShaderSrcTexture2D =
      "precision mediump float;\n"
      "uniform sampler2D u_texture_y;\n"
      "uniform sampler2D u_texture_uv;\n"
      "varying vec2 v_texCoord;\n"
      "void main() {\n"
      "  gl_FragColor.r = texture2D(u_texture_y, v_texCoord).r;\n"
      "  gl_FragColor.gb = texture2D(u_texture_uv, v_texCoord).rg;\n"
      "  gl_FragColor.a = 1.0;\n"
      "}\n";

  gl::GLApi* api = gl::g_current_gl_context;

  GLint status = 0;
  GLuint vertex_shader = api->glCreateShaderFn(GL_VERTEX_SHADER);
  SCOPED_GL_CLEANUP_VAR(api, DeleteShader, vertex_shader);
  ASSERT_NE(vertex_shader, 0u);
  api->glShaderSourceFn(vertex_shader, 1, &kVertexShaderSrc, nullptr);
  api->glCompileShaderFn(vertex_shader);
  api->glGetShaderivFn(vertex_shader, GL_COMPILE_STATUS, &status);
  ASSERT_NE(status, 0);

  GLuint fragment_shader = api->glCreateShaderFn(GL_FRAGMENT_SHADER);
  SCOPED_GL_CLEANUP_VAR(api, DeleteShader, fragment_shader);
  ASSERT_NE(fragment_shader, 0u);
  api->glShaderSourceFn(fragment_shader, 1,
                        use_factory ? &kVideoFragmentShaderSrcTexture2D
                                    : &kVideoFragmentShaderSrcTextureExternal,
                        nullptr);
  api->glCompileShaderFn(fragment_shader);
  api->glGetShaderivFn(fragment_shader, GL_COMPILE_STATUS, &status);
  ASSERT_NE(status, 0);

  GLuint program = api->glCreateProgramFn();
  ASSERT_NE(program, 0u);
  SCOPED_GL_CLEANUP_VAR(api, DeleteProgram, program);
  api->glAttachShaderFn(program, vertex_shader);
  api->glAttachShaderFn(program, fragment_shader);
  api->glLinkProgramFn(program);
  api->glGetProgramivFn(program, GL_LINK_STATUS, &status);
  ASSERT_NE(status, 0);

  GLint vertex_location = api->glGetAttribLocationFn(program, "a_position");
  ASSERT_NE(vertex_location, -1);

  GLint y_texture_location =
      api->glGetUniformLocationFn(program, "u_texture_y");
  ASSERT_NE(y_texture_location, -1);

  GLint uv_texture_location =
      api->glGetUniformLocationFn(program, "u_texture_uv");
  ASSERT_NE(uv_texture_location, -1);

  GLuint fbo, renderbuffer = 0u;
  api->glGenFramebuffersEXTFn(1, &fbo);
  ASSERT_NE(fbo, 0u);
  SCOPED_GL_CLEANUP_PTR(api, DeleteFramebuffersEXT, 1, fbo);
  api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo);

  api->glGenRenderbuffersEXTFn(1, &renderbuffer);
  ASSERT_NE(renderbuffer, 0u);
  SCOPED_GL_CLEANUP_PTR(api, DeleteRenderbuffersEXT, 1, renderbuffer);
  api->glBindRenderbufferEXTFn(GL_RENDERBUFFER, renderbuffer);

  api->glRenderbufferStorageEXTFn(GL_RENDERBUFFER, GL_RGBA8_OES, size.width(),
                                  size.height());
  api->glFramebufferRenderbufferEXTFn(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_RENDERBUFFER, renderbuffer);
  ASSERT_EQ(api->glCheckFramebufferStatusEXTFn(GL_FRAMEBUFFER),
            static_cast<unsigned>(GL_FRAMEBUFFER_COMPLETE));

  // Set the clear color to green.
  api->glViewportFn(0, 0, size.width(), size.height());
  api->glClearColorFn(0.0f, 1.0f, 0.0f, 1.0f);
  api->glClearFn(GL_COLOR_BUFFER_BIT);

  GLuint vbo = 0u;
  api->glGenBuffersARBFn(1, &vbo);
  ASSERT_NE(vbo, 0u);
  SCOPED_GL_CLEANUP_PTR(api, DeleteBuffersARB, 1, vbo);
  api->glBindBufferFn(GL_ARRAY_BUFFER, vbo);
  static const float vertices[] = {
      1.0f, 1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
      1.0f, 1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
  };
  api->glBufferDataFn(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
                      GL_STATIC_DRAW);

  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  // Create the representations for the planes, get the texture ids, bind to
  // samplers, and draw.
  auto texture =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          shared_image_ref->mailbox());
  ASSERT_NE(texture, nullptr);

  auto texture_access = texture->BeginScopedAccess(
      GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  ASSERT_NE(texture_access, nullptr);

  api->glActiveTextureFn(GL_TEXTURE0);
  api->glBindTextureFn(
      use_factory ? GL_TEXTURE_2D : GL_TEXTURE_EXTERNAL_OES,
      texture->GetTexturePassthrough(/*plane_index=*/0)->service_id());
  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  api->glActiveTextureFn(GL_TEXTURE1);
  api->glBindTextureFn(
      use_factory ? GL_TEXTURE_2D : GL_TEXTURE_EXTERNAL_OES,
      texture->GetTexturePassthrough(/*plane_index=*/1)->service_id());
  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  api->glUseProgramFn(program);

  api->glEnableVertexAttribArrayFn(vertex_location);
  api->glVertexAttribPointerFn(vertex_location, 2, GL_FLOAT, GL_FALSE, 0,
                               nullptr);

  api->glUniform1iFn(y_texture_location, 0);
  api->glUniform1iFn(uv_texture_location, 1);

  api->glDrawArraysFn(GL_TRIANGLES, 0, 6);
  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  GLubyte pixel_color[4];
  api->glReadPixelsFn(size.width() / 2, size.height() / 2, 1, 1, GL_RGBA,
                      GL_UNSIGNED_BYTE, pixel_color);
  EXPECT_EQ(kYFillValue, pixel_color[0]);
  EXPECT_EQ(kUFillValue, pixel_color[1]);
  EXPECT_EQ(kVFillValue, pixel_color[2]);
  EXPECT_EQ(255, pixel_color[3]);
  // TODO(dawn:551): Test Dawn access after multi-planar support lands in Dawn.
}

TEST_P(D3DImageBackingFactoryTest, CreateFromVideoTexture) {
  RunVideoTest(/*use_shared_handle=*/false, /*use_factory=*/false);
}

TEST_P(D3DImageBackingFactoryTest, CreateFromVideoTextureSharedHandle) {
  RunVideoTest(/*use_shared_handle=*/true, /*use_factory=*/false);
}

TEST_P(D3DImageBackingFactoryTest,
       CreateFromVideoTextureViaFactoryMultiplanar) {
  RunVideoTest(/*use_shared_handle=*/true,
               /*use_factory=*/true);
}

void D3DImageBackingFactoryTest::RunOverlayTest(bool use_shared_handle,
                                                bool use_factory) {
  constexpr gfx::Size size(32, 32);

  constexpr uint8_t kYFillValue = 0x12;
  constexpr uint8_t kUFillValue = 0x23;
  constexpr uint8_t kVFillValue = 0x34;

  auto shared_image_ref =
      CreateVideoImage(size, kYFillValue, kUFillValue, kVFillValue,
                       use_shared_handle, use_factory);
  ASSERT_TRUE(shared_image_ref);

  auto overlay_representation =
      shared_image_representation_factory_->ProduceOverlay(
          shared_image_ref->mailbox());

  auto scoped_read_access = overlay_representation->BeginScopedReadAccess();
  ASSERT_TRUE(scoped_read_access);

  std::optional<gl::DCLayerOverlayImage> overlay_image =
      scoped_read_access->GetDCLayerOverlayImage();
  ASSERT_TRUE(overlay_image);
  EXPECT_EQ(overlay_image->type(), gl::DCLayerOverlayType::kD3D11Texture);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      shared_image_factory_->GetDeviceForTesting();

  CD3D11_TEXTURE2D_DESC staging_desc(
      DXGI_FORMAT_NV12, size.width(), size.height(), 1, 1, 0,
      D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture;
  HRESULT hr =
      d3d11_device->CreateTexture2D(&staging_desc, nullptr, &staging_texture);
  ASSERT_EQ(hr, S_OK);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
  d3d11_device->GetImmediateContext(&device_context);

  device_context->CopyResource(staging_texture.Get(),
                               overlay_image->d3d11_video_texture());
  D3D11_MAPPED_SUBRESOURCE mapped_resource = {};
  hr = device_context->Map(staging_texture.Get(), 0, D3D11_MAP_READ, 0,
                           &mapped_resource);
  ASSERT_EQ(hr, S_OK);

  CheckNV12(static_cast<const uint8_t*>(mapped_resource.pData),
            mapped_resource.RowPitch, size, kYFillValue, kUFillValue,
            kVFillValue);

  device_context->Unmap(staging_texture.Get(), 0);
}

TEST_P(D3DImageBackingFactoryTest, CreateFromVideoTextureOverlay) {
  RunOverlayTest(/*use_shared_handle=*/false, /*use_factory=*/false);
}

TEST_P(D3DImageBackingFactoryTest, CreateFromVideoTextureSharedHandleOverlay) {
  RunOverlayTest(/*use_shared_handle=*/true, /*use_factory=*/false);
}

TEST_P(D3DImageBackingFactoryTest,
       CreateFromVideoTextureViaFactoryMultiplanarOverlay) {
  RunOverlayTest(/*use_shared_handle=*/true,
                 /*use_factory=*/true);
}

void D3DImageBackingFactoryTest::RunCreateFromSharedMemoryMultiplanarTest(
    bool use_async_copy) {
  constexpr gfx::Size size(32, 32);
  constexpr size_t kDataSize = size.width() * size.height() * 3 / 2;

  const gpu::Mailbox mailbox = gpu::Mailbox::Generate();

  // This test writes to the created SharedImages via GL and then reads back
  // those contents via GL for verification.
  constexpr gpu::SharedImageUsageSet usage =
      gpu::SHARED_IMAGE_USAGE_VIDEO_DECODE |
      gpu::SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE |
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto shm_region = base::UnsafeSharedMemoryRegion::Create(kDataSize);
  {
    base::WritableSharedMemoryMapping shm_mapping = shm_region.Map();
    FillNV12(shm_mapping.GetMemoryAs<uint8_t>(), size, 255, 255, 255);
  }

  gfx::GpuMemoryBufferHandle shm_gmb_handle(shm_region.Duplicate());
  DCHECK(shm_gmb_handle.region().IsValid());
  shm_gmb_handle.stride = size.width();

  // CompoundImageBacking wrapping D3DImageBacking is required for shared
  // memory support.
  auto backing = CompoundImageBacking::CreateSharedMemoryForTesting(
      shared_image_factory_.get(), copy_manager_, mailbox,
      std::move(shm_gmb_handle), viz::MultiPlaneFormat::kNV12, size,
      gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
      "TestLabel");
  EXPECT_NE(backing, nullptr);

  EXPECT_EQ(backing->mailbox(), mailbox);
  EXPECT_EQ(backing->size(), size);
  EXPECT_EQ(backing->format(), viz::MultiPlaneFormat::kNV12);
  EXPECT_EQ(backing->color_space(), gfx::ColorSpace());
  EXPECT_EQ(backing->surface_origin(), kTopLeft_GrSurfaceOrigin);
  EXPECT_EQ(backing->alpha_type(), kPremul_SkAlphaType);
  EXPECT_EQ(backing->usage(), usage);
  EXPECT_TRUE(backing->IsCleared());

  auto shared_image_ref = shared_image_manager_.Register(
      std::move(backing), memory_type_tracker_.get());

  constexpr uint8_t kYClearValue = 0x12;
  constexpr uint8_t kUClearValue = 0x23;
  constexpr uint8_t kVClearValue = 0x34;

  gl::GLApi* api = gl::g_current_gl_context;

  GLuint fbo;
  api->glGenFramebuffersEXTFn(1, &fbo);
  ASSERT_NE(fbo, 0u);
  SCOPED_GL_CLEANUP_PTR(api, DeleteFramebuffersEXT, 1, fbo);
  api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo);

  auto gl_representation =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
  ASSERT_NE(gl_representation, nullptr);

  auto gl_access = gl_representation->BeginScopedAccess(
      GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  ASSERT_NE(gl_access, nullptr);

  GLuint y_texture_id =
      gl_representation->GetTexturePassthrough(/*plane_index=*/0)->service_id();
  api->glBindTextureFn(GL_TEXTURE_2D, y_texture_id);
  api->glFramebufferTexture2DEXTFn(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, y_texture_id, 0);
  ASSERT_EQ(api->glCheckFramebufferStatusEXTFn(GL_FRAMEBUFFER),
            static_cast<unsigned>(GL_FRAMEBUFFER_COMPLETE));
  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  GLubyte y_value;
  api->glReadPixelsFn(size.width() / 2, size.height() / 2, 1, 1, GL_RED,
                      GL_UNSIGNED_BYTE, &y_value);
  EXPECT_EQ(255, y_value);

  api->glViewportFn(0, 0, size.width(), size.height());
  api->glClearColorFn(kYClearValue / 255.0f, 0, 0, 0);
  api->glClearFn(GL_COLOR_BUFFER_BIT);

  api->glReadPixelsFn(size.width() / 2, size.height() / 2, 1, 1, GL_RED,
                      GL_UNSIGNED_BYTE, &y_value);
  EXPECT_EQ(kYClearValue, y_value);

  GLuint uv_texture_id =
      gl_representation->GetTexturePassthrough(/*plane_index=*/1)->service_id();
  api->glBindTextureFn(GL_TEXTURE_2D, uv_texture_id);
  api->glFramebufferTexture2DEXTFn(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, uv_texture_id, 0);
  ASSERT_EQ(api->glCheckFramebufferStatusEXTFn(GL_FRAMEBUFFER),
            static_cast<unsigned>(GL_FRAMEBUFFER_COMPLETE));
  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  GLubyte uv_value[2];
  api->glReadPixelsFn(size.width() / 4, size.height() / 4, 1, 1, GL_RG,
                      GL_UNSIGNED_BYTE, uv_value);
  EXPECT_EQ(255, uv_value[0]);
  EXPECT_EQ(255, uv_value[1]);

  api->glViewportFn(0, 0, size.width(), size.height());
  api->glClearColorFn(kUClearValue / 255.0f, kVClearValue / 255.0f, 0, 0);
  api->glClearFn(GL_COLOR_BUFFER_BIT);

  api->glReadPixelsFn(size.width() / 4, size.height() / 4, 1, 1, GL_RG,
                      GL_UNSIGNED_BYTE, uv_value);
  EXPECT_EQ(kUClearValue, uv_value[0]);
  EXPECT_EQ(kVClearValue, uv_value[1]);

  gl_access.reset();
  gl_representation.reset();

  if (use_async_copy) {
    base::RunLoop run_loop;
    shared_image_ref->CopyToGpuMemoryBufferAsync(
        base::BindLambdaForTesting([&](bool succeeded) {
            EXPECT_TRUE(succeeded);
            run_loop.Quit();
        }));
    run_loop.Run();
  } else {
    EXPECT_TRUE(shared_image_ref->CopyToGpuMemoryBuffer());
  }

  {
    base::WritableSharedMemoryMapping shm_mapping = shm_region.Map();
    CheckNV12(shm_mapping.GetMemoryAs<uint8_t>(), size.width(), size,
              kYClearValue, kUClearValue, kVClearValue);
  }

  {
    auto overlay_representation =
        shared_image_representation_factory_->ProduceOverlay(mailbox);

    auto scoped_read_access = overlay_representation->BeginScopedReadAccess();
    ASSERT_TRUE(scoped_read_access);

    std::optional<gl::DCLayerOverlayImage> overlay_image =
        scoped_read_access->GetDCLayerOverlayImage();
    ASSERT_TRUE(overlay_image);
    EXPECT_EQ(overlay_image->type(), gl::DCLayerOverlayType::kShMemPixmap);

    CheckNV12(overlay_image->shm_video_pixmap().data(),
              overlay_image->pixmap_stride(), size, kYClearValue, kUClearValue,
              kVClearValue);
  }
}

TEST_P(D3DImageBackingFactoryTest, CreateFromSharedMemoryMultiplanar) {
    RunCreateFromSharedMemoryMultiplanarTest(/*use_async_copy=*/false);
}

TEST_P(D3DImageBackingFactoryTest, CreateFromSharedMemoryMultiplanarAsyncCopy) {
    RunCreateFromSharedMemoryMultiplanarTest(/*use_async_copy=*/true);
}

// Verifies that a multi-planar NV12 image can be created without DXGI handle
// for use with software GMBs.
void D3DImageBackingFactoryTest::RunMultiplanarUploadAndReadback(
    bool use_update_subresource) {
  base::test::ScopedFeatureList feature_list;
  if (use_update_subresource) {
    feature_list.InitAndEnableFeature(
        features::kD3DBackingUploadWithUpdateSubresource);
  } else {
    feature_list.InitAndDisableFeature(
        features::kD3DBackingUploadWithUpdateSubresource);
  }

  // Recreate a new share image factory which will pick up the
  // kD3DBackingUploadWithUpdateSubresource feature.
  shared_image_factory_ = std::make_unique<D3DImageBackingFactory>(
      gl::QueryD3D11DeviceObjectFromANGLE(),
      shared_image_manager_.dxgi_shared_handle_manager(), GLFormatCaps());

  constexpr gfx::Size size(32, 32);
  constexpr size_t kDataSize = size.width() * size.height() * 3 / 2;
  constexpr SkAlphaType alpha_type = kPremul_SkAlphaType;
  constexpr gfx::ColorSpace color_space;
  constexpr gpu::SharedImageUsageSet usage =
      gpu::SHARED_IMAGE_USAGE_RASTER_READ |
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_CPU_UPLOAD;
  constexpr auto format = viz::MultiPlaneFormat::kNV12;
  const gpu::Mailbox mailbox = gpu::Mailbox::Generate();

  auto owned_backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, kNullSurfaceHandle, size, color_space,
      kTopLeft_GrSurfaceOrigin, alpha_type, usage, "TestLabel",
      /*is_thread_safe=*/false);
  ASSERT_NE(owned_backing, nullptr);
  SharedImageBacking* backing = owned_backing.get();

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image_ref =
      shared_image_manager_.Register(std::move(owned_backing),
                                     memory_type_tracker_.get());
  ASSERT_TRUE(shared_image_ref);

  constexpr uint8_t kInitialY = 255;
  constexpr uint8_t kInitialU = 255;
  constexpr uint8_t kInitialV = 0;

  std::vector<uint8_t> buffer(kDataSize);
  FillNV12(buffer.data(), size, kInitialY, kInitialU, kInitialV);

  // Make pixmaps that point to each plane for use with upload/readback.
  std::vector<SkPixmap> pixmaps;
  {
    size_t plane_offset = 0;
    for (int plane = 0; plane < format.NumberOfPlanes(); ++plane) {
      gfx::Size plane_size = format.GetPlaneSize(plane, size);
      auto info = SkImageInfo::Make(gfx::SizeToSkISize(plane_size),
                                    viz::ToClosestSkColorType(format, plane),
                                    alpha_type, color_space.ToSkColorSpace());
      DCHECK_LE(info.computeMinByteSize() + plane_offset, kDataSize);
      pixmaps.emplace_back(info, buffer.data() + plane_offset,
                           info.minRowBytes());
      plane_offset += info.computeMinByteSize();
    }
  }

  // Upload initial data into the image.
  backing->UploadFromMemory(pixmaps);
  backing->SetCleared();

  // If UpdateSubresource() is used, the staging texture shouldn't be created.
  EXPECT_EQ(
      !static_cast<D3DImageBacking*>(backing)->HasStagingTextureForTesting(),
      use_update_subresource);

  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
  ASSERT_TRUE(skia_representation);

  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
      scoped_read_access =
          skia_representation->BeginScopedReadAccess(nullptr, nullptr);
  ASSERT_TRUE(scoped_read_access);

  // Using glReadPixels() to check each plane has expected data after upload
  // doesn't work due to https://anglebug.com/7998. Instead draw from NV12
  // textures into a RGBA texture and readback from the RGBA texture.
  SkImageInfo rgba_image_info =
      SkImageInfo::Make(size.width(), size.height(), kRGBA_8888_SkColorType,
                        alpha_type, color_space.ToSkColorSpace());

  SkBitmap dst_bitmap;
  dst_bitmap.allocPixels(rgba_image_info);
  {
    auto source_image = scoped_read_access->CreateSkImage(context_state_.get());
    ASSERT_TRUE(source_image);

    sk_sp<SkSurface> dest_surface;
    SkSurfaceProps surface_props(0, kUnknown_SkPixelGeometry);
    if (context_state_->gr_context()) {
      dest_surface = SkSurfaces::RenderTarget(
          context_state_->gr_context(), skgpu::Budgeted::kNo, rgba_image_info,
          0, kTopLeft_GrSurfaceOrigin, &surface_props);
    } else {
      DCHECK(context_state_->gpu_main_graphite_recorder());
      dest_surface = SkSurfaces::RenderTarget(
          context_state_->gpu_main_graphite_recorder(), rgba_image_info,
          skgpu::Mipmapped::kNo, &surface_props);
    }
    ASSERT_NE(dest_surface, nullptr);

    {
      auto* canvas = dest_surface->getCanvas();
      SkPaint paint;
      paint.setBlendMode(SkBlendMode::kSrc);

      canvas->drawImageRect(source_image, gfx::RectToSkRect(gfx::Rect(size)),
                            gfx::RectToSkRect(gfx::Rect(size)),
                            SkSamplingOptions(), &paint,
                            SkCanvas::kStrict_SrcRectConstraint);
    }

    sk_sp<SkImage> dst_image;
    if (context_state_->gr_context()) {
      GrBackendTexture backend_texture = SkSurfaces::GetBackendTexture(
          dest_surface.get(), SkSurfaces::BackendHandleAccess::kFlushWrite);
      dst_image = SkImages::BorrowTextureFrom(
          context_state_->gr_context(), backend_texture,
          kTopLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, alpha_type,
          nullptr);
    } else {
      dst_image = SkSurfaces::AsImage(dest_surface);
    }
    ASSERT_TRUE(dst_image);

    EXPECT_TRUE(ReadPixels(dst_image, rgba_image_info, dst_bitmap.getPixels(),
                           rgba_image_info.minRowBytes(), 0, 0));
  }

  // YUV(255, 255, 0) maps to RGB(255, 74, 255).
  SkColor expected_rgba_color = SkColorSetARGB(255, 74, 255, 255);

  SkBitmap expected_bitmap;
  expected_bitmap.allocPixels(rgba_image_info);
  expected_bitmap.eraseColor(expected_rgba_color);

  EXPECT_TRUE(cc::MatchesBitmap(dst_bitmap, expected_bitmap,
                                cc::ExactPixelComparator()));

  // Clear out `buffer` and then readback into it and verify YUV(255, 255, 0)
  // was read back.
  FillNV12(buffer.data(), size, 0, 0, 0);
  ASSERT_TRUE(backing->ReadbackToMemory(pixmaps));
  CheckNV12(buffer.data(), size.width(), size, kInitialY, kInitialU, kInitialV);
}

TEST_P(D3DImageBackingFactoryTest, MultiplanarUploadAndReadback) {
  RunMultiplanarUploadAndReadback(/*use_update_subresource=*/false);
}

TEST_P(D3DImageBackingFactoryTest,
       MultiplanarUploadAndReadbackWithUpdateSubresource) {
  RunMultiplanarUploadAndReadback(/*use_update_subresource=*/true);
}

TEST_P(D3DImageBackingFactoryTest, CanCreateScanoutBacking) {
  const gfx::Size arbitrary_size = gfx::Size(4, 4);

  // E.g. a hardware decoded video frame
  EXPECT_TRUE(shared_image_factory_->CanCreateSharedImage(
      gpu::SHARED_IMAGE_USAGE_VIDEO_DECODE | gpu::SHARED_IMAGE_USAGE_SCANOUT,
      viz::MultiPlaneFormat::kNV12, arbitrary_size, /*thread_safe=*/false,
      gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE, GrContextType ::kGL, {}));

  // E.g. getDisplayMedia content, like a shared window
  EXPECT_TRUE(shared_image_factory_->CanCreateSharedImage(
      gpu::SHARED_IMAGE_USAGE_SCANOUT, viz::MultiPlaneFormat::kNV12,
      arbitrary_size, /*thread_safe=*/false,
      gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE, GrContextType ::kGL, {}));

  // E.g. delegated compositing draw quad resources
  EXPECT_EQ(
      gl::DirectCompositionTextureSupported(),
      shared_image_factory_->CanCreateSharedImage(
          gpu::SHARED_IMAGE_USAGE_SCANOUT, viz::SinglePlaneFormat::kBGRA_8888,
          arbitrary_size, /*thread_safe=*/false,
          gfx::GpuMemoryBufferType::EMPTY_BUFFER, GrContextType ::kGL, {}));
}

TEST_P(D3DImageBackingFactoryTest, CanProduceDCompTextureOverlay) {
  if (!gl::DirectCompositionTextureSupported()) {
    GTEST_SKIP() << "IDCompositionTexture not supported";
  }

  constexpr gfx::Size size(32, 32);
  constexpr SkAlphaType alpha_type = kPremul_SkAlphaType;
  constexpr gfx::ColorSpace color_space;
  constexpr gpu::SharedImageUsageSet usage =
      gpu::SHARED_IMAGE_USAGE_RASTER_READ |
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;
  constexpr auto format = viz::SinglePlaneFormat::kBGRA_8888;
  const gpu::Mailbox mailbox = gpu::Mailbox::Generate();

  auto owned_backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, kNullSurfaceHandle, size, color_space,
      kTopLeft_GrSurfaceOrigin, alpha_type, usage, "TestLabel",
      /*is_thread_safe=*/false);
  ASSERT_NE(owned_backing, nullptr);
  SharedImageBacking* backing = owned_backing.get();

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image_ref =
      shared_image_manager_.Register(std::move(owned_backing),
                                     memory_type_tracker_.get());
  ASSERT_TRUE(shared_image_ref);

  // Mark it as initialized-- we won't be reading the actual pixels.
  backing->SetCleared();

  auto overlay_representation =
      shared_image_representation_factory_->ProduceOverlay(mailbox);

  auto scoped_read_access = overlay_representation->BeginScopedReadAccess();
  ASSERT_TRUE(scoped_read_access) << "Can begin overlay read access";
  auto overlay_image = scoped_read_access->GetDCLayerOverlayImage();
  ASSERT_TRUE(overlay_image);
  ASSERT_TRUE(overlay_image->dcomp_visual_content())
      << "Overlay image is visual content";
  Microsoft::WRL::ComPtr<IUnknown> visual_content =
      overlay_image->dcomp_visual_content();
  Microsoft::WRL::ComPtr<IDCompositionTexture> dcomp_texture;
  ASSERT_HRESULT_SUCCEEDED(visual_content.As(&dcomp_texture))
      << "Overlay image visual content is a DComp texture";
  ASSERT_TRUE(dcomp_texture);
}

TEST_P(D3DImageBackingFactoryTest, CanProduceVideoForExternalDevice) {
  constexpr gfx::Size size(32, 32);
  constexpr SkAlphaType alpha_type = kPremul_SkAlphaType;
  constexpr gfx::ColorSpace color_space;
  constexpr gpu::SharedImageUsageSet usage =
      gpu::SHARED_IMAGE_USAGE_RASTER_READ |
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  constexpr auto format = viz::SinglePlaneFormat::kBGRA_8888;
  const gpu::Mailbox mailbox = gpu::Mailbox::Generate();

  auto owned_backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, kNullSurfaceHandle, size, color_space,
      kTopLeft_GrSurfaceOrigin, alpha_type, usage, "TestLabel",
      /*is_thread_safe=*/false);
  ASSERT_NE(owned_backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image_ref =
      shared_image_manager_.Register(std::move(owned_backing),
                                     memory_type_tracker_.get());
  ASSERT_TRUE(shared_image_ref);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
  UINT creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
  static const D3D_FEATURE_LEVEL feature_levels[] = {
      D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,
      D3D_FEATURE_LEVEL_9_1};
  HRESULT hr =
      D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, creation_flags,
                        feature_levels, std::size(feature_levels),
                        D3D11_SDK_VERSION, &d3d11_device, nullptr, nullptr);
  if (FAILED(hr)) {
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, 0, creation_flags,
                           feature_levels, std::size(feature_levels),
                           D3D11_SDK_VERSION, &d3d11_device, nullptr, nullptr);
  }
  if (FAILED(hr)) {
    GTEST_SKIP()
        << " cannot produce a d3d video representation on this platform";
  }

  auto representation = shared_image_manager_.ProduceVideo(
      d3d11_device, mailbox, memory_type_tracker_.get());
  EXPECT_NE(representation, nullptr);
  auto read_access = representation->BeginScopedReadAccess();
  D3D11TextureAndArrayIndex input_texture = read_access->GetD3D11Texture();
  EXPECT_NE(input_texture.texture, nullptr);
  EXPECT_EQ(input_texture.array_index, 0u);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    D3DImageBackingFactoryTest,
    ::testing::Values(GrContextType::kGL, GrContextType::kGraphiteDawn),
    [](const testing::TestParamInfo<GrContextType>& info) {
      switch (info.param) {
        case GrContextType::kGL:
          return "GaneshGL";
        case GrContextType::kGraphiteDawn:
          return "GraphiteDawn";
        default:
          // This should not be reached with the provided values.
          return "Unknown";
      }
    });


class D3DImageBackingFactoryBufferTest : public D3DImageBackingFactoryTestBase {
 public:
  void SetUp() override {
    D3DImageBackingFactoryTestBase::SetUp();
  }

 protected:
  static constexpr wgpu::FeatureName kRequiredFeatures[] = {
      // Required for Dawn interop.
      wgpu::FeatureName::SharedFenceDXGISharedHandle,
      wgpu::FeatureName::SharedBufferMemoryD3D12Resource,
  };

  void CheckDawnBuffer(wgpu::Buffer src_buffer,
                       const wgpu::Instance& instance,
                       const wgpu::Device& device,
                       const uint32_t size,
                       const uint32_t expected_value) const {
    wgpu::BufferDescriptor buffer_desc{
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
        .size = size};
    wgpu::Buffer dst_buffer = device.CreateBuffer(&buffer_desc);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToBuffer(src_buffer, 0, dst_buffer, 0, size);
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);

    wgpu::FutureWaitInfo wait_info{
        dst_buffer.MapAsync(wgpu::MapMode::Read, 0, buffer_desc.size,
                            wgpu::CallbackMode::WaitAnyOnly,
                            [&](wgpu::MapAsyncStatus status, wgpu::StringView) {
                              ASSERT_EQ(status, wgpu::MapAsyncStatus::Success);
                            })};
    wgpu::WaitStatus status =
        instance.WaitAny(1, &wait_info, std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(status, wgpu::WaitStatus::Success);

    const uint32_t* dst_value =
        reinterpret_cast<const uint32_t*>(dst_buffer.GetConstMappedRange());

    EXPECT_EQ(expected_value, *dst_value);
  }
};

// Verifies that creating a shared image backed by a D3D12 buffer works and can
// be imported into Dawn.
TEST_F(D3DImageBackingFactoryBufferTest, CreateSharedImageImportToDawn) {
  static constexpr auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
  wgpu::InstanceDescriptor instance_desc = {
      .requiredFeatureCount = 1,
      .requiredFeatures = &kTimedWaitAny,
  };
  dawn::native::Instance instance(&instance_desc);
  wgpu::RequestAdapterOptions adapter_options;
  adapter_options.backendType = wgpu::BackendType::D3D12;

  // allow_unsafe_apis is required to use Dawn's SharedBufferMemory feature.
  std::array<const char*, 1> adapter_enabled_toggles;
  adapter_enabled_toggles[0] = "allow_unsafe_apis";

  wgpu::DawnTogglesDescriptor adapter_toggles_desc;
  adapter_toggles_desc.enabledToggles = adapter_enabled_toggles.data();
  adapter_toggles_desc.enabledToggleCount = adapter_enabled_toggles.size();
  adapter_options.nextInChain = &adapter_toggles_desc;

  std::vector<dawn::native::Adapter> adapters =
      instance.EnumerateAdapters(&adapter_options);
  ASSERT_GT(adapters.size(), 0u);

  wgpu::DeviceDescriptor device_descriptor;
  device_descriptor.requiredFeatureCount = std::size(kRequiredFeatures);
  device_descriptor.requiredFeatures = kRequiredFeatures;

  wgpu::Device device =
      wgpu::Device::Acquire(adapters[0].CreateDevice(&device_descriptor));

  constexpr uint32_t kBufferSize = 4;
  const gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, viz::SharedImageFormat(), gpu::kNullSurfaceHandle,
      gfx::Size(kBufferSize, 1), gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin,
      kUnknown_SkAlphaType,
      gpu::SHARED_IMAGE_USAGE_WEBGPU_READ |
          gpu::SHARED_IMAGE_USAGE_WEBGPU_WRITE |
          gpu::SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER,
      "TestLabel", false);

  // Register the backing and create a wgpu::Buffer from it.
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  auto dawn_representation =
      shared_image_representation_factory_->ProduceDawnBuffer(
          mailbox, device, wgpu::BackendType::D3D12, /*context_state=*/nullptr);
  ASSERT_NE(dawn_representation, nullptr);

  auto scoped_access = dawn_representation->BeginScopedAccess(
      wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst);
  ASSERT_TRUE(scoped_access);

  wgpu::Buffer buffer(scoped_access->buffer());

  // Create and upload data to a mappable buffer.
  wgpu::BufferDescriptor buffer_desc{
      .usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::MapWrite,
      .size = kBufferSize,
      .mappedAtCreation = true};
  wgpu::Buffer src_buffer = device.CreateBuffer(&buffer_desc);

  constexpr uint32_t kBufferData = 0x12345678;
  uint32_t* mapped_range = static_cast<uint32_t*>(src_buffer.GetMappedRange());
  std::memcpy(mapped_range, &kBufferData, kBufferSize);
  src_buffer.Unmap();

  // Copy data from the mappable buffer to the imported buffer.
  wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
  encoder.CopyBufferToBuffer(src_buffer, 0, buffer, 0, kBufferSize);
  wgpu::CommandBuffer commands = encoder.Finish();

  wgpu::Queue queue = device.GetQueue();
  queue.Submit(1, &commands);

  // Check that the contents of the imported D3D12 resource within Dawn.
  CheckDawnBuffer(buffer, instance.Get(), device, kBufferSize, kBufferData);

  factory_ref.reset();
}

// Disabled by default as it requires DX11.
TEST_F(D3DImageBackingFactoryTest, DISABLED_CreateGpuMemoryBuffer) {
  for (auto format : {
           viz::SinglePlaneFormat::kRGBA_8888,
           viz::SinglePlaneFormat::kRGBX_8888,
           viz::SinglePlaneFormat::kBGRA_8888,
           viz::SinglePlaneFormat::kBGRX_8888,
       }) {
    gfx::BufferUsage usages[] = {
        gfx::BufferUsage::GPU_READ,
        gfx::BufferUsage::SCANOUT,
    };
    for (auto usage : usages) {
      gfx::GpuMemoryBufferHandle handle =
          D3DImageBackingFactory::CreateGpuMemoryBufferHandle(
              /*io_runner=*/nullptr, gfx::Size(2, 2), format, usage);
      EXPECT_EQ(handle.type, gfx::DXGI_SHARED_HANDLE);
    }
  }
}

}  // namespace gpu
