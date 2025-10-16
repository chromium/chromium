// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/image_decode_accelerator_proxy.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ref.h"
#include "base/test/task_environment.h"
#include "cc/paint/paint_image.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/mock_gpu_channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;

namespace gpu {
namespace {

constexpr int kChannelId = 5;

class TestGpuChannelHost : public GpuChannelHost {
 public:
  explicit TestGpuChannelHost(mojom::GpuChannel& gpu_channel)
      : TestGpuChannelHost(gpu_channel, GPUInfo()) {}

  TestGpuChannelHost(mojom::GpuChannel& gpu_channel, const GPUInfo& info)
      : GpuChannelHost(kChannelId,
                       info,
                       GpuFeatureInfo(),
                       SharedImageCapabilities(),
                       mojo::ScopedMessagePipeHandle(
                           mojo::MessagePipeHandle(mojo::kInvalidHandleValue))),
        gpu_channel_(gpu_channel) {}

  mojom::GpuChannel& GetGpuChannel() override { return *gpu_channel_; }

 protected:
  ~TestGpuChannelHost() override = default;

 private:
  const raw_ref<mojom::GpuChannel> gpu_channel_;
};

class ImageDecodeAcceleratorProxyTest : public ::testing::Test {
 public:
  ImageDecodeAcceleratorProxyTest()
      : gpu_channel_host_(
            base::MakeRefCounted<TestGpuChannelHost>(mock_gpu_channel_)),
        proxy_(gpu_channel_host_.get(),
               static_cast<int32_t>(
                   GpuChannelReservedRoutes::kImageDecodeAccelerator)) {}

  ~ImageDecodeAcceleratorProxyTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockGpuChannel mock_gpu_channel_;
  scoped_refptr<TestGpuChannelHost> gpu_channel_host_;
  ImageDecodeAcceleratorProxy proxy_;
};

class ImageDecodeAcceleratorProxySubsamplingTest
    : public testing::TestWithParam<cc::YUVSubsampling> {
 public:
  ImageDecodeAcceleratorProxySubsamplingTest() = default;
  ~ImageDecodeAcceleratorProxySubsamplingTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockGpuChannel mock_gpu_channel_;
};

TEST_P(ImageDecodeAcceleratorProxySubsamplingTest, JPEGSubsamplingIsSupported) {
  cc::ImageHeaderMetadata image_metadata;
  image_metadata.yuv_subsampling = GetParam();
  image_metadata.image_type = cc::ImageType::kJPEG;
  image_metadata.all_data_received_prior_to_decode = true;
  image_metadata.has_embedded_color_profile = false;
  image_metadata.image_size = gfx::Size(100, 200);
  image_metadata.jpeg_is_progressive = false;

  ImageDecodeAcceleratorSupportedProfile profile;
  profile.image_type = ImageDecodeAcceleratorType::kJpeg;
  profile.min_encoded_dimensions = gfx::Size(0, 0);
  profile.max_encoded_dimensions = gfx::Size(1920, 1080);

  static_assert(
      // TODO(andrescj): refactor to instead have a static_assert at the
      // declaration site of ImageDecodeAcceleratorSubsampling to make sure it
      // has the same number of entries as cc::YUVSubsampling.
      static_cast<int>(ImageDecodeAcceleratorSubsampling::kMaxValue) == 2,
      "ImageDecodeAcceleratorProxySubsamplingTest.JPEGSubsamplingIsSupported "
      "must be adapted to support all subsampling factors in "
      "ImageDecodeAcceleratorSubsampling");
  switch (GetParam()) {
    case cc::YUVSubsampling::k420:
      profile.subsamplings.push_back(ImageDecodeAcceleratorSubsampling::k420);
      break;
    case cc::YUVSubsampling::k422:
      profile.subsamplings.push_back(ImageDecodeAcceleratorSubsampling::k422);
      break;
    case cc::YUVSubsampling::k444:
      profile.subsamplings.push_back(ImageDecodeAcceleratorSubsampling::k444);
      break;
    default:
      return;
  }
  GPUInfo gpu_info;
  gpu_info.image_decode_accelerator_supported_profiles.push_back(profile);

  auto gpu_channel_host =
      base::MakeRefCounted<TestGpuChannelHost>(mock_gpu_channel_, gpu_info);
  ImageDecodeAcceleratorProxy proxy(
      gpu_channel_host.get(),
      static_cast<int32_t>(GpuChannelReservedRoutes::kImageDecodeAccelerator));

  EXPECT_TRUE(proxy.IsImageSupported(&image_metadata));
}

TEST_P(ImageDecodeAcceleratorProxySubsamplingTest,
       JPEGSubsamplingIsNotSupported) {
  cc::ImageHeaderMetadata image_metadata;
  image_metadata.yuv_subsampling = GetParam();
  image_metadata.image_type = cc::ImageType::kJPEG;
  image_metadata.all_data_received_prior_to_decode = true;
  image_metadata.has_embedded_color_profile = false;
  image_metadata.image_size = gfx::Size(100, 200);
  image_metadata.jpeg_is_progressive = false;

  ImageDecodeAcceleratorSupportedProfile profile;
  profile.image_type = ImageDecodeAcceleratorType::kJpeg;
  profile.min_encoded_dimensions = gfx::Size(0, 0);
  profile.max_encoded_dimensions = gfx::Size(1920, 1080);

  static_assert(
      // TODO(andrescj): refactor to instead have a static_assert at the
      // declaration site of ImageDecodeAcceleratorSubsampling to make sure it
      // has the same number of entries as cc::YUVSubsampling.
      static_cast<int>(ImageDecodeAcceleratorSubsampling::kMaxValue) == 2,
      "ImageDecodeAcceleratorProxySubsamplingTest.JPEGSubsamplingIsNotSupported"
      "must be adapted to support all subsampling factors in "
      "ImageDecodeAcceleratorSubsampling");

  // Advertise support for all subsamplings except the GetParam() one.
  if (GetParam() != cc::YUVSubsampling::k420)
    profile.subsamplings.push_back(ImageDecodeAcceleratorSubsampling::k420);
  if (GetParam() != cc::YUVSubsampling::k422)
    profile.subsamplings.push_back(ImageDecodeAcceleratorSubsampling::k422);
  if (GetParam() != cc::YUVSubsampling::k444)
    profile.subsamplings.push_back(ImageDecodeAcceleratorSubsampling::k444);

  GPUInfo gpu_info;
  gpu_info.image_decode_accelerator_supported_profiles.push_back(profile);

  auto gpu_channel_host =
      base::MakeRefCounted<TestGpuChannelHost>(mock_gpu_channel_, gpu_info);
  ImageDecodeAcceleratorProxy proxy(
      gpu_channel_host.get(),
      static_cast<int32_t>(GpuChannelReservedRoutes::kImageDecodeAccelerator));

  EXPECT_FALSE(proxy.IsImageSupported(&image_metadata));
}

INSTANTIATE_TEST_SUITE_P(ImageDecodeAcceleratorProxySubsample,
                         ImageDecodeAcceleratorProxySubsamplingTest,
                         testing::Values(cc::YUVSubsampling::k410,
                                         cc::YUVSubsampling::k411,
                                         cc::YUVSubsampling::k420,
                                         cc::YUVSubsampling::k422,
                                         cc::YUVSubsampling::k440,
                                         cc::YUVSubsampling::k444,
                                         cc::YUVSubsampling::kUnknown));

}  // namespace
}  // namespace gpu
