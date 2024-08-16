// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <va/va.h>

#include <memory>
#include <utility>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/service/image_decode_accelerator_worker.h"
#include "media/gpu/vaapi/vaapi_image_decode_accelerator_worker.h"
#include "media/gpu/vaapi/vaapi_image_decoder.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap_handle.h"

using testing::_;
using testing::AllOf;
using testing::InSequence;
using testing::IsNull;
using testing::NotNull;
using testing::Property;
using testing::Return;
using testing::StrictMock;

namespace media {
namespace {

constexpr gfx::BufferFormat kFormatForDecodes = gfx::BufferFormat::YVU_420;

constexpr gfx::Size kVaSurfaceResolution(128, 256);

constexpr gfx::Size kVisibleSize(120, 250);

constexpr size_t kWebPFileAndVp8ChunkHeaderSizeInBytes = 20u;

// clang-format off
constexpr uint8_t kJpegPFileHeader[] = {0xFF, 0xD8, 0xFF};

constexpr uint8_t kLossyWebPFileHeader[] = {
    'R', 'I', 'F', 'F',
    0x0c, 0x00, 0x00, 0x00,  // == 12 (little endian)
    'W', 'E', 'B', 'P',
    'V', 'P', '8', ' ',
    0x00, 0x00, 0x00, 0x00  // == 0
};
// clang-format on

constexpr base::span<const uint8_t, 3u> kJpegEncodedData = kJpegPFileHeader;

constexpr base::span<const uint8_t, kWebPFileAndVp8ChunkHeaderSizeInBytes>
    kLossyWebPEncodedData = kLossyWebPFileHeader;

class MockNativePixmapDmaBuf : public gfx::NativePixmapDmaBuf {
 public:
  MockNativePixmapDmaBuf(const gfx::Size& size)
      : gfx::NativePixmapDmaBuf(size,
                                kFormatForDecodes,
                                gfx::NativePixmapHandle()) {}

  gfx::NativePixmapHandle ExportHandle() const override {
    gfx::NativePixmapHandle handle{};
    DCHECK_EQ(gfx::BufferFormat::YVU_420, GetBufferFormat());
    handle.planes = std::vector<gfx::NativePixmapPlane>(3u);
    return handle;
  }

 protected:
  ~MockNativePixmapDmaBuf() override = default;
};

class MockVaapiImageDecoder : public VaapiImageDecoder {
 public:
  MockVaapiImageDecoder(gpu::ImageDecodeAcceleratorType type)
      : VaapiImageDecoder(VAProfileNone), type_(type) {}
  ~MockVaapiImageDecoder() override = default;

  gpu::ImageDecodeAcceleratorType GetType() const override { return type_; }
  SkYUVColorSpace GetYUVColorSpace() const override {
    switch (type_) {
      case gpu::ImageDecodeAcceleratorType::kJpeg:
        return SkYUVColorSpace::kJPEG_SkYUVColorSpace;
      case gpu::ImageDecodeAcceleratorType::kWebP:
        return SkYUVColorSpace::kRec601_SkYUVColorSpace;
      case gpu::ImageDecodeAcceleratorType::kUnknown:
        NOTREACHED();
    }
  }

  MOCK_METHOD1(Initialize, bool(const ReportErrorToUMACB&));
  MOCK_METHOD1(Decode, VaapiImageDecodeStatus(base::span<const uint8_t>));
  MOCK_CONST_METHOD0(GetScopedVASurface, const ScopedVASurface*());
  MOCK_METHOD1(
      ExportAsNativePixmapDmaBuf,
      std::unique_ptr<NativePixmapAndSizeInfo>(VaapiImageDecodeStatus*));
  MOCK_METHOD1(AllocateVASurfaceAndSubmitVABuffers,
               VaapiImageDecodeStatus(base::span<const uint8_t>));

 private:
  const gpu::ImageDecodeAcceleratorType type_;
};

}  // namespace

class VaapiImageDecodeAcceleratorWorkerTest : public testing::Test {
 public:
  VaapiImageDecodeAcceleratorWorkerTest() {
    feature_list_.InitWithFeatures(
        {features::kVaapiJpegImageDecodeAcceleration,
         features::kVaapiWebPImageDecodeAcceleration} /* enabled_features */,
        {} /* disabled_features */);
    VaapiImageDecoderVector decoders;
    gpu::ImageDecodeAcceleratorSupportedProfiles supported_profiles;

    auto fake_jpeg_profile =
        GetFakeSupportedProfile(gpu::ImageDecodeAcceleratorType::kJpeg);
    supported_profiles.push_back(fake_jpeg_profile);
    auto fake_webp_profile =
        GetFakeSupportedProfile(gpu::ImageDecodeAcceleratorType::kWebP);
    supported_profiles.push_back(fake_webp_profile);

    auto vaapi_jpeg_decoder =
        std::make_unique<StrictMock<MockVaapiImageDecoder>>(
            gpu::ImageDecodeAcceleratorType::kJpeg);
    vaapi_jpeg_decoder_ = vaapi_jpeg_decoder.get();
    decoders.push_back(std::move(vaapi_jpeg_decoder));

    auto vaapi_webp_decoder =
        std::make_unique<StrictMock<MockVaapiImageDecoder>>(
            gpu::ImageDecodeAcceleratorType::kWebP);
    vaapi_webp_decoder_ = vaapi_webp_decoder.get();
    decoders.push_back(std::move(vaapi_webp_decoder));

    worker_ = base::WrapUnique(new VaapiImageDecodeAcceleratorWorker(
        std::move(decoders), std::move(supported_profiles)));
  }

  VaapiImageDecodeAcceleratorWorkerTest(
      const VaapiImageDecodeAcceleratorWorkerTest&) = delete;
  VaapiImageDecodeAcceleratorWorkerTest& operator=(
      const VaapiImageDecodeAcceleratorWorkerTest&) = delete;

  gpu::ImageDecodeAcceleratorSupportedProfile GetFakeSupportedProfile(
      gpu::ImageDecodeAcceleratorType type) {
    gpu::ImageDecodeAcceleratorSupportedProfile profile;
    profile.image_type = type;
    return profile;
  }

  StrictMock<MockVaapiImageDecoder>* GetJpegDecoder() const {
    return vaapi_jpeg_decoder_;
  }

  StrictMock<MockVaapiImageDecoder>* GetWebPDecoder() const {
    return vaapi_webp_decoder_;
  }

  MOCK_METHOD1(
      OnDecodeCompleted,
      void(std::unique_ptr<gpu::ImageDecodeAcceleratorWorker::DecodeResult>));

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<VaapiImageDecodeAcceleratorWorker> worker_;

  raw_ptr<StrictMock<MockVaapiImageDecoder>> vaapi_jpeg_decoder_ = nullptr;
  raw_ptr<StrictMock<MockVaapiImageDecoder>> vaapi_webp_decoder_ = nullptr;
};

ACTION_P2(ExportAsNativePixmapDmaBufSuccessfully,
          va_surface_resolution,
          visible_size) {
  *arg0 = VaapiImageDecodeStatus::kSuccess;
  auto exported_pixmap = std::make_unique<NativePixmapAndSizeInfo>();
  exported_pixmap->va_surface_resolution = va_surface_resolution;
  exported_pixmap->byte_size = 1u;
  exported_pixmap->pixmap =
      base::MakeRefCounted<MockNativePixmapDmaBuf>(visible_size);
  return exported_pixmap;
}

TEST_F(VaapiImageDecodeAcceleratorWorkerTest, ImageDecodeSucceeds) {
  std::vector<uint8_t> jpeg_encoded_data(kJpegEncodedData.begin(),
                                         kJpegEncodedData.end());
  std::vector<uint8_t> webp_encoded_data(kLossyWebPEncodedData.begin(),
                                         kLossyWebPEncodedData.end());
  {
    InSequence sequence;
    MockVaapiImageDecoder* jpeg_decoder = GetJpegDecoder();
    ASSERT_TRUE(jpeg_decoder);
    EXPECT_CALL(*jpeg_decoder, Initialize(_)).WillOnce(Return(true));
    EXPECT_CALL(
        *jpeg_decoder,
        Decode(AllOf(Property(&base::span<const uint8_t>::data,
                              jpeg_encoded_data.data()),
                     Property(&base::span<const uint8_t>::size,
                              jpeg_encoded_data.size())) /* encoded_data */))
        .WillOnce(Return(VaapiImageDecodeStatus::kSuccess));
    EXPECT_CALL(*jpeg_decoder,
                ExportAsNativePixmapDmaBuf(NotNull() /* status */))
        .WillOnce(ExportAsNativePixmapDmaBufSuccessfully(kVaSurfaceResolution,
                                                         kVisibleSize));
    EXPECT_CALL(*this, OnDecodeCompleted(NotNull()));

    MockVaapiImageDecoder* webp_decoder = GetWebPDecoder();
    ASSERT_TRUE(webp_decoder);
    EXPECT_CALL(*webp_decoder, Initialize(_)).WillOnce(Return(true));
    EXPECT_CALL(
        *webp_decoder,
        Decode(AllOf(Property(&base::span<const uint8_t>::data,
                              webp_encoded_data.data()),
                     Property(&base::span<const uint8_t>::size,
                              webp_encoded_data.size())) /* encoded_data */))
        .WillOnce(Return(VaapiImageDecodeStatus::kSuccess));
    EXPECT_CALL(*webp_decoder,
                ExportAsNativePixmapDmaBuf(NotNull() /* status */))
        .WillOnce(ExportAsNativePixmapDmaBufSuccessfully(kVaSurfaceResolution,
                                                         kVisibleSize));
    EXPECT_CALL(*this, OnDecodeCompleted(NotNull()));
  }

  worker_->Decode(
      std::move(jpeg_encoded_data), kVisibleSize,
      base::BindOnce(&VaapiImageDecodeAcceleratorWorkerTest::OnDecodeCompleted,
                     base::Unretained(this)));

  worker_->Decode(
      std::move(webp_encoded_data), kVisibleSize,
      base::BindOnce(&VaapiImageDecodeAcceleratorWorkerTest::OnDecodeCompleted,
                     base::Unretained(this)));
  task_environment_.RunUntilIdle();
}

TEST_F(VaapiImageDecodeAcceleratorWorkerTest, ImageDecodeFails) {
  std::vector<uint8_t> jpeg_encoded_data(kJpegEncodedData.begin(),
                                         kJpegEncodedData.end());
  std::vector<uint8_t> webp_encoded_data(kLossyWebPEncodedData.begin(),
                                         kLossyWebPEncodedData.end());
  {
    InSequence sequence;
    MockVaapiImageDecoder* jpeg_decoder = GetJpegDecoder();
    ASSERT_TRUE(jpeg_decoder);
    EXPECT_CALL(*jpeg_decoder, Initialize(_)).WillOnce(Return(true));
    EXPECT_CALL(
        *jpeg_decoder,
        Decode(AllOf(Property(&base::span<const uint8_t>::data,
                              jpeg_encoded_data.data()),
                     Property(&base::span<const uint8_t>::size,
                              jpeg_encoded_data.size())) /* encoded_data */))
        .WillOnce(Return(VaapiImageDecodeStatus::kExecuteDecodeFailed));
    EXPECT_CALL(*this, OnDecodeCompleted(IsNull()));

    MockVaapiImageDecoder* webp_decoder = GetWebPDecoder();
    ASSERT_TRUE(webp_decoder);
    EXPECT_CALL(*webp_decoder, Initialize(_)).WillOnce(Return(true));
    EXPECT_CALL(
        *webp_decoder,
        Decode(AllOf(Property(&base::span<const uint8_t>::data,
                              webp_encoded_data.data()),
                     Property(&base::span<const uint8_t>::size,
                              webp_encoded_data.size())) /* encoded_data */))
        .WillOnce(Return(VaapiImageDecodeStatus::kExecuteDecodeFailed));
    EXPECT_CALL(*this, OnDecodeCompleted(IsNull()));
  }

  worker_->Decode(
      std::move(jpeg_encoded_data), kVisibleSize,
      base::BindOnce(&VaapiImageDecodeAcceleratorWorkerTest::OnDecodeCompleted,
                     base::Unretained(this)));

  worker_->Decode(
      std::move(webp_encoded_data), kVisibleSize,
      base::BindOnce(&VaapiImageDecodeAcceleratorWorkerTest::OnDecodeCompleted,
                     base::Unretained(this)));
  task_environment_.RunUntilIdle();
}

TEST_F(VaapiImageDecodeAcceleratorWorkerTest, UnknownImageDecodeFails) {
  std::vector<uint8_t> encoded_data = {1u, 2u, 3u};
  EXPECT_CALL(*this, OnDecodeCompleted(IsNull()));
  worker_->Decode(
      std::move(encoded_data), kVisibleSize,
      base::BindOnce(&VaapiImageDecodeAcceleratorWorkerTest::OnDecodeCompleted,
                     base::Unretained(this)));
  task_environment_.RunUntilIdle();
}

}  // namespace media
