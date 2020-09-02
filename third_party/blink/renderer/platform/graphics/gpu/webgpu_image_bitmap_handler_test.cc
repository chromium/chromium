// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_image_bitmap_handler.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/null_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/client/webgpu_interface_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"

using testing::_;
using testing::Return;

namespace blink {

namespace {
gpu::SyncToken GenTestSyncToken(GLbyte id) {
  gpu::SyncToken token;
  // Store id in the first byte
  reinterpret_cast<GLbyte*>(&token)[0] = id;
  return token;
}

scoped_refptr<StaticBitmapImage> CreateBitmap() {
  auto mailbox = gpu::Mailbox::GenerateForSharedImage();
  auto release_callback = viz::SingleReleaseCallback::Create(
      base::BindOnce([](const gpu::SyncToken&, bool) {}));
  return AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
      mailbox, GenTestSyncToken(100), 0, SkImageInfo::MakeN32Premul(100, 100),
      GL_TEXTURE_2D, true, SharedGpuContext::ContextProviderWrapper(),
      base::PlatformThread::CurrentRef(),
      base::MakeRefCounted<base::NullTaskRunner>(),
      std::move(release_callback));
}

bool GPUUploadingPathSupported() {
// In current state, only passthrough command buffer can work on this path.
// and Windows is the platform that is using passthrough command buffer by
// default.
// TODO(shaobo.yan@intel.com): Enable test on more platforms when they're ready.
#if defined(OS_WIN)
  return true;
#else
  return false;
#endif  // defined(OS_WIN)
}

class MockWebGPUInterface : public gpu::webgpu::WebGPUInterfaceStub {
 public:
  MOCK_METHOD(gpu::webgpu::ReservedTexture,
              ReserveTexture,
              (uint64_t device_client_id));
  MOCK_METHOD(void,
              AssociateMailbox,
              (GLuint64 device_client_id,
               GLuint device_generation,
               GLuint id,
               GLuint generation,
               GLuint usage,
               const GLbyte* mailbox));
  MOCK_METHOD(void,
              DissociateMailbox,
              (GLuint64 device_client_id,
               GLuint texture_id,
               GLuint texture_generation));
};

// The six reference pixels are: red, green, blue, white, black.
static const uint8_t rgba8[] = {
    0xFF, 0x00, 0x00, 0xFF,  // Red
    0x00, 0xFF, 0x00, 0xFF,  // Green
    0x00, 0x00, 0xFF, 0xFF,  // Blue
    0x00, 0x00, 0x00, 0xFF,  // White
    0xFF, 0xFF, 0xFF, 0xFF,  // Opaque Black
    0xFF, 0xFF, 0xFF, 0x00,  // Transparent Black
};

static const uint8_t bgra8[] = {
    0x00, 0x00, 0xFF, 0xFF,  // Red
    0x00, 0xFF, 0x00, 0xFF,  // Green
    0xFF, 0x00, 0x00, 0xFF,  // Blue
    0x00, 0x00, 0x00, 0xFF,  // White
    0xFF, 0xFF, 0xFF, 0xFF,  // Opaque Black
    0xFF, 0xFF, 0xFF, 0x00,  // Transparent Black
};

static const uint8_t rgb10a2[] = {
    0xFF, 0x03, 0x00, 0xC0,  // Red
    0x00, 0xFC, 0x0F, 0xC0,  // Green
    0x00, 0x00, 0xF0, 0xFF,  // Blue
    0x00, 0x00, 0x00, 0xC0,  // White
    0xFF, 0xFF, 0xFF, 0xFF,  // Opaque Black
    0xFF, 0xFF, 0xFF, 0x3F,  // Transparent Black
};

static const uint16_t f16[] = {
    0x3C00, 0x0000, 0x0000, 0x3C00,  // Red
    0x0000, 0x3C00, 0x0000, 0x3C00,  // Green
    0x0000, 0x0000, 0x3C00, 0x3C00,  // Blue
    0x0000, 0x0000, 0x0000, 0x3C00,  // White
    0x3C00, 0x3C00, 0x3C00, 0x3C00,  // Opaque Black
    0x3C00, 0x3C00, 0x3C00, 0x0000,  // Transparent Black
};

static const float f32[] = {
    1.0f, 0.0f, 0.0f, 1.0f,  // Red
    0.0f, 1.0f, 0.0f, 1.0f,  // Green
    0.0f, 0.0f, 1.0f, 1.0f,  // Blue
    0.0f, 0.0f, 0.0f, 1.0f,  // White
    1.0f, 1.0f, 1.0f, 1.0f,  // Opaque Black
    1.0f, 1.0f, 1.0f, 0.0f,  // Transparent Black
};

static const uint8_t rg8[] = {
    0xFF, 0x00,  // Red
    0x00, 0xFF,  // Green
    0x00, 0x00,  // No Blue
    0x00, 0x00,  // White
    0xFF, 0xFF,  // Opaque Black
    0xFF, 0xFF,  // Transparent Black
};

static const uint16_t rg16f[] = {
    0x3C00, 0x0000,  // Red
    0x0000, 0x3C00,  // Green
    0x0000, 0x0000,  // No Blue
    0x0000, 0x0000,  // White
    0x3C00, 0x3C00,  // Opaque Black
    0x3C00, 0x3C00,  // Transparent Black
};

base::span<const uint8_t> GetDstContent(WGPUTextureFormat format) {
  switch (format) {
    case WGPUTextureFormat_RG8Unorm:
      return base::span<const uint8_t>(rg8, sizeof(rg8));
    case WGPUTextureFormat_RGBA8Unorm:
    // We need to ensure no color space conversion happens
    // during imageBitmap uploading.
    case WGPUTextureFormat_RGBA8UnormSrgb:
      return base::span<const uint8_t>(rgba8, sizeof(rgba8));
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
      return base::span<const uint8_t>(bgra8, sizeof(bgra8));
    case WGPUTextureFormat_RGB10A2Unorm:
      return base::span<const uint8_t>(rgb10a2, sizeof(rgb10a2));
    case WGPUTextureFormat_RG16Float:
      return base::span<const uint8_t>(reinterpret_cast<const uint8_t*>(rg16f),
                                       sizeof(rg16f));
    case WGPUTextureFormat_RGBA16Float:
      return base::span<const uint8_t>(reinterpret_cast<const uint8_t*>(f16),
                                       sizeof(f16));
    case WGPUTextureFormat_RGBA32Float:
      return base::span<const uint8_t>(reinterpret_cast<const uint8_t*>(f32),
                                       sizeof(f32));
    default:
      NOTREACHED();
      return {};
  }
}

base::span<const uint8_t> GetSrcPixelContent(SkColorType format) {
  switch (format) {
    case SkColorType::kRGBA_8888_SkColorType:
      return base::span<const uint8_t>(rgba8, sizeof(rgba8));
    case SkColorType::kBGRA_8888_SkColorType:
      return base::span<const uint8_t>(bgra8, sizeof(bgra8));
    case SkColorType::kRGBA_F16_SkColorType:
      return base::span<const uint8_t>(reinterpret_cast<const uint8_t*>(f16),
                                       sizeof(f16));
    default:
      NOTREACHED();
      return {};
  }
}

}  // anonymous namespace

class WebGPUImageBitmapHandlerTest : public testing::Test {
 protected:
  void SetUp() override {}

  void VerifyCopyBytesForCanvasColorParams(uint64_t width,
                                           uint64_t height,
                                           SkImageInfo info,
                                           IntRect copy_rect,
                                           WGPUTextureFormat color_type) {
    const uint64_t content_length = width * height * info.bytesPerPixel();
    std::vector<uint8_t> contents(content_length, 0);
    // Initialize contents.
    for (size_t i = 0; i < content_length; ++i) {
      contents[i] = i % std::numeric_limits<uint8_t>::max();
    }

    VerifyCopyBytes(width, height, info, copy_rect, color_type,
                    base::span<uint8_t>(contents.data(), content_length),
                    base::span<uint8_t>(contents.data(), content_length));
  }

  void VerifyCopyBytes(uint64_t width,
                       uint64_t height,
                       SkImageInfo info,
                       IntRect copy_rect,
                       WGPUTextureFormat color_type,
                       base::span<const uint8_t> contents,
                       base::span<const uint8_t> expected_value) {
    uint64_t bytes_per_pixel = DawnTextureFormatBytesPerPixel(color_type);
    ASSERT_EQ(contents.size(), width * height * info.bytesPerPixel());
    sk_sp<SkData> image_pixels =
        SkData::MakeWithCopy(contents.data(), contents.size());
    scoped_refptr<StaticBitmapImage> image =
        StaticBitmapImage::Create(std::move(image_pixels), info);

    WebGPUImageUploadSizeInfo wgpu_info =
        ComputeImageBitmapWebGPUUploadSizeInfo(copy_rect, color_type);

    const uint64_t result_length = wgpu_info.size_in_bytes;
    std::vector<uint8_t> results(result_length, 0);
    bool success = CopyBytesFromImageBitmapForWebGPU(
        image, base::span<uint8_t>(results.data(), result_length), copy_rect,
        color_type);
    ASSERT_EQ(success, true);

    // Compare content and results
    uint32_t bytes_per_row = wgpu_info.wgpu_bytes_per_row;
    uint32_t content_row_index =
        (copy_rect.Y() * width + copy_rect.X()) * bytes_per_pixel;
    uint32_t result_row_index = 0;
    for (int i = 0; i < copy_rect.Height(); ++i) {
      EXPECT_EQ(0, memcmp(&expected_value[content_row_index],
                          &results[result_row_index],
                          copy_rect.Width() * bytes_per_pixel));
      content_row_index += width * bytes_per_pixel;
      result_row_index += bytes_per_row;
    }
  }
};

TEST_F(WebGPUImageBitmapHandlerTest, VerifyColorConvert) {
  // All supported CanvasPixelFormat mapping to SkColorType
  const SkColorType srcSkColorFormat[] = {
      SkColorType::kRGBA_8888_SkColorType,
      SkColorType::kBGRA_8888_SkColorType,
      SkColorType::kRGBA_F16_SkColorType,
  };

  // Joint of SkColorType and WebGPU texture format
  const WGPUTextureFormat kDstWebGPUTextureFormat[] = {
      WGPUTextureFormat_RG16Float,      WGPUTextureFormat_RGBA16Float,
      WGPUTextureFormat_RGBA32Float,

      WGPUTextureFormat_RGB10A2Unorm,   WGPUTextureFormat_RG8Unorm,
      WGPUTextureFormat_RGBA8Unorm,     WGPUTextureFormat_BGRA8Unorm,
      WGPUTextureFormat_RGBA8UnormSrgb, WGPUTextureFormat_BGRA8UnormSrgb,
  };

  const CanvasColorSpace kColorSpaces[] = {
      CanvasColorSpace::kSRGB,
      CanvasColorSpace::kRec2020,
      CanvasColorSpace::kP3,
  };

  uint64_t kImageWidth = 3;
  uint64_t kImageHeight = 2;

  IntRect image_data_rect(0, 0, kImageWidth, kImageHeight);

  for (SkColorType src_color_type : srcSkColorFormat) {
    for (WGPUTextureFormat dst_color_type : kDstWebGPUTextureFormat) {
      for (CanvasColorSpace color_space : kColorSpaces) {
        SkImageInfo info =
            SkImageInfo::Make(kImageWidth, kImageHeight, src_color_type,
                              SkAlphaType::kUnpremul_SkAlphaType,
                              CanvasColorSpaceToSkColorSpace(color_space));
        VerifyCopyBytes(kImageWidth, kImageHeight, info, image_data_rect,
                        dst_color_type, GetSrcPixelContent(src_color_type),
                        GetDstContent(dst_color_type));
      }
    }
  }
}

// Test calculate size
TEST_F(WebGPUImageBitmapHandlerTest, VerifyGetWGPUResourceInfo) {
  uint64_t kImageWidth = 63;
  uint64_t kImageHeight = 1;

  // Prebaked expected values.
  uint32_t expected_bytes_per_row = 256;
  uint64_t expected_size = 256;

  IntRect test_rect(0, 0, kImageWidth, kImageHeight);
  WebGPUImageUploadSizeInfo info = ComputeImageBitmapWebGPUUploadSizeInfo(
      test_rect, WGPUTextureFormat_RGBA8Unorm);
  ASSERT_EQ(expected_size, info.size_in_bytes);
  ASSERT_EQ(expected_bytes_per_row, info.wgpu_bytes_per_row);
}

// Copy full image bitmap test
TEST_F(WebGPUImageBitmapHandlerTest, VerifyCopyBytesFromImageBitmapForWebGPU) {
  uint64_t kImageWidth = 4;
  uint64_t kImageHeight = 2;
  SkImageInfo info = SkImageInfo::Make(
      kImageWidth, kImageHeight, SkColorType::kRGBA_8888_SkColorType,
      SkAlphaType::kUnpremul_SkAlphaType, SkColorSpace::MakeSRGB());

  IntRect image_data_rect(0, 0, kImageWidth, kImageHeight);
  VerifyCopyBytesForCanvasColorParams(kImageWidth, kImageHeight, info,
                                      image_data_rect,
                                      WGPUTextureFormat_RGBA8Unorm);
}

// Copy sub image bitmap test
TEST_F(WebGPUImageBitmapHandlerTest, VerifyCopyBytesFromSubImageBitmap) {
  uint64_t kImageWidth = 63;
  uint64_t kImageHeight = 4;
  SkImageInfo info = SkImageInfo::Make(
      kImageWidth, kImageHeight, SkColorType::kRGBA_8888_SkColorType,
      SkAlphaType::kUnpremul_SkAlphaType, SkColorSpace::MakeSRGB());

  IntRect image_data_rect(2, 2, 60, 2);
  VerifyCopyBytesForCanvasColorParams(kImageWidth, kImageHeight, info,
                                      image_data_rect,
                                      WGPUTextureFormat_RGBA8Unorm);
}

// Copy image bitmap with premultiply alpha
TEST_F(WebGPUImageBitmapHandlerTest, VerifyCopyBytesWithPremultiplyAlpha) {
  uint64_t kImageWidth = 2;
  uint64_t kImageHeight = 1;
  SkImageInfo info = SkImageInfo::Make(
      kImageWidth, kImageHeight, SkColorType::kRGBA_8888_SkColorType,
      SkAlphaType::kPremul_SkAlphaType, SkColorSpace::MakeSRGB());

  IntRect image_data_rect(0, 0, 2, 1);
  VerifyCopyBytesForCanvasColorParams(kImageWidth, kImageHeight, info,
                                      image_data_rect,
                                      WGPUTextureFormat_RGBA8Unorm);
}

class DawnTextureFromImageBitmapTest : public testing::Test {
 protected:
  void SetUp() override {
    auto webgpu = std::make_unique<MockWebGPUInterface>();
    webgpu_ = webgpu.get();
    auto provider = std::make_unique<WebGraphicsContext3DProviderForTests>(
        std::move(webgpu));

    dawn_control_client_ =
        base::MakeRefCounted<DawnControlClientHolder>(std::move(provider));

    dawn_texture_provider_ = base::MakeRefCounted<DawnTextureFromImageBitmap>(
        dawn_control_client_, 1 /* device_client_id */);

    test_context_provider_ = viz::TestContextProvider::Create();
    InitializeSharedGpuContext(test_context_provider_.get());
  }

  void TearDown() override { SharedGpuContext::ResetForTesting(); }
  MockWebGPUInterface* webgpu_;
  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  scoped_refptr<DawnTextureFromImageBitmap> dawn_texture_provider_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(DawnTextureFromImageBitmapTest, VerifyAccessTexture) {
  if (!GPUUploadingPathSupported()) {
    LOG(ERROR) << "Test skipped because GPU uploading path not supported.";
    return;
  }
  auto bitmap = CreateBitmap();

  viz::TransferableResource resource;
  gpu::webgpu::ReservedTexture reservation = {
      reinterpret_cast<WGPUTexture>(&resource), 1, 1};

  // Test that ProduceDawnTextureFromImageBitmap calls ReserveTexture and
  // AssociateMailbox correctly.
  const GLbyte* mailbox_bytes = nullptr;

  EXPECT_CALL(*webgpu_, ReserveTexture(_)).WillOnce(Return(reservation));
  EXPECT_CALL(*webgpu_, AssociateMailbox(
                            dawn_texture_provider_->GetDeviceClientIdForTest(),
                            _, reservation.id, reservation.generation,
                            WGPUTextureUsage_CopySrc, _))
      .WillOnce(testing::SaveArg<5>(&mailbox_bytes));

  WGPUTexture texture =
      dawn_texture_provider_->ProduceDawnTextureFromImageBitmap(bitmap);

  gpu::Mailbox mailbox = gpu::Mailbox::FromVolatile(
      *reinterpret_cast<const volatile gpu::Mailbox*>(mailbox_bytes));

  EXPECT_TRUE(mailbox == bitmap->GetMailboxHolder().mailbox);
  EXPECT_NE(texture, nullptr);
  EXPECT_EQ(dawn_texture_provider_->GetTextureIdForTest(), 1u);
  EXPECT_EQ(dawn_texture_provider_->GetTextureGenerationForTest(), 1u);

  // Test that FinishDawnTextureFromImageBitmapAccess calls DissociateMailbox
  // correctly.
  EXPECT_CALL(*webgpu_, DissociateMailbox(
                            dawn_texture_provider_->GetDeviceClientIdForTest(),
                            reservation.id, reservation.generation));

  dawn_texture_provider_->FinishDawnTextureFromImageBitmapAccess();

  EXPECT_EQ(dawn_texture_provider_->GetTextureIdForTest(), 0u);
}
}  // namespace blink
