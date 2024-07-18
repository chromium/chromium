// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// gtest.h has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include "base/bits.h"
#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/memory_mapped_file.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/task_environment.h"
#include "base/test/test_suite.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_helpers.h"
#include "media/base/video_codecs.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_stateful_video_decoder.h"
#include "media/gpu/v4l2/v4l2_utils.h"
#include "third_party/libdrm/src/include/drm/drm_fourcc.h"
#include "ui/gfx/linux/gbm_defines.h"

#include <drm.h>
#include <fcntl.h>
#include <gbm.h>
#include <string.h>
#include <xf86drm.h>

namespace media {

namespace {

const base::FilePath kDecoderDevicePrefix("/dev/dri/");

#define TOSTR(enumCase) \
  case enumCase:        \
    return #enumCase

struct DrmVersionDeleter {
  void operator()(drmVersion* version) const { drmFreeVersion(version); }
};
using ScopedDrmVersionPtr = std::unique_ptr<drmVersion, DrmVersionDeleter>;

// Converts v4l2 format to gbm format
uint32_t ToGBMFormat(uint32_t v4l2_format) {
  if (v4l2_format == V4L2_PIX_FMT_NV12 || v4l2_format == V4L2_PIX_FMT_NV12M) {
    return DRM_FORMAT_NV12;
  }
  return DRM_FORMAT_INVALID;
}

const char* VideoCodecProfileToString(VideoCodecProfile profile) {
  switch (profile) {
    TOSTR(H264PROFILE_BASELINE);
    TOSTR(H264PROFILE_MAIN);
    TOSTR(H264PROFILE_EXTENDED);
    TOSTR(H264PROFILE_HIGH);
    TOSTR(VP8PROFILE_ANY);
    TOSTR(VP9PROFILE_PROFILE0);
    TOSTR(AV1PROFILE_PROFILE_MAIN);
    default:
      return "profile_not_enumerated";
  }
}

}  // namespace

class V4L2MinigbmTest
    : public testing::TestWithParam<std::tuple<VideoCodecProfile, gfx::Size>> {
 public:
  V4L2MinigbmTest() = default;
  ~V4L2MinigbmTest() = default;

  struct PrintToStringParamName {
    template <class ParamType>
    std::string operator()(
        const testing::TestParamInfo<ParamType>& info) const {
      return base::StringPrintf(
          "%s__%s", VideoCodecProfileToString(std::get<0>(info.param)),
          std::get<1>(info.param).ToString().c_str());
    }
  };
};

void TestStatefulDecoderAllocations(uint32_t codec_fourcc,
                                    scoped_refptr<V4L2Device> device,
                                    uint32_t chosen_v4l2_pixel_format,
                                    gfx::Size resolution) {
  scoped_refptr<V4L2Queue> OUTPUT_queue =
      device->GetQueue(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  ASSERT_NE(OUTPUT_queue.get(), nullptr);
  const std::optional<struct v4l2_format> input_v4l2_format =
      OUTPUT_queue->SetFormat(codec_fourcc, gfx::Size(), /*buffer_size=*/1E6);
  ASSERT_TRUE(input_v4l2_format.has_value());

  // Checks that pixel format is set properly as denoted in section 4.5.1.5
  // https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/dev-decoder.html#initialization.
  ASSERT_EQ(codec_fourcc, input_v4l2_format.value().fmt.pix_mp.pixelformat)
      << "The driver should never changed the codec :)";
  LOG(INFO) << " Chosen codec: " << FourccToString(codec_fourcc);
  constexpr size_t kNumInputBuffers = 8;
  ASSERT_NE(
      OUTPUT_queue->AllocateBuffers(kNumInputBuffers, V4L2_MEMORY_MMAP, false),
      0u);

  scoped_refptr<V4L2Queue> CAPTURE_queue =
      device->GetQueue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  ASSERT_NE(CAPTURE_queue.get(), nullptr);
  std::optional<struct v4l2_format> output_v4l2_format =
      CAPTURE_queue->SetFormat(chosen_v4l2_pixel_format, resolution,
                               /*buffer_size=*/0);
  ASSERT_TRUE(output_v4l2_format.has_value());
  const gfx::Size coded_size(output_v4l2_format->fmt.pix_mp.width,
                             output_v4l2_format->fmt.pix_mp.height);
  LOG_IF(INFO, resolution != coded_size)
      << "Device adjusted " << resolution.ToString() << " to "
      << coded_size.ToString();
  constexpr size_t kNumOutputBuffers = VIDEO_MAX_FRAME;
  ASSERT_NE(CAPTURE_queue->AllocateBuffers(kNumOutputBuffers, V4L2_MEMORY_MMAP,
                                           false),
            0u);

  // Determines proper device driver to use for setting up GBM.
  base::FilePath drm_path;
  base::FileEnumerator fe(kDecoderDevicePrefix, true,
                          base::FileEnumerator::FILES);

  for (base::FilePath name = fe.Next(); !name.empty(); name = fe.Next()) {
    base::File fd(name, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ScopedDrmVersionPtr version(drmGetVersion(fd.GetPlatformFile()));

    // VGEM in the version name describes a virtual driver which
    // is not what is desired for the tests.
    if (strncmp(version->name, "vgem", 4)) {
      drm_path = name;
      break;
    }
  }

  ASSERT_TRUE(!drm_path.empty());
  base::File drm_fd(drm_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                  base::File::FLAG_WRITE);
  ASSERT_TRUE(drm_fd.IsValid());
  struct gbm_device* gbm = gbm_create_device(drm_fd.GetPlatformFile());
  ASSERT_TRUE(gbm);

  const auto gbm_format = ToGBMFormat(chosen_v4l2_pixel_format);
  ASSERT_NE(gbm_format, static_cast<uint32_t>(DRM_FORMAT_INVALID));

  struct gbm_bo* bo = gbm_bo_create(
      gbm, coded_size.width(), coded_size.height(), gbm_format,
      GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING | GBM_BO_USE_HW_VIDEO_DECODER);
  ASSERT_TRUE(bo);

  EXPECT_EQ(coded_size,
            gfx::Size(base::checked_cast<int>(gbm_bo_get_width(bo)),
                      base::checked_cast<int>(gbm_bo_get_height(bo))));

  // Minigbm currently rounds up the stride to the nearest multiple of 64
  // while the Compute Strides function only rounds to the nearest multiple
  // of 32. Round device value to nearest multiple of 64 and compare
  // stride values.
  const int bo_num_planes = gbm_bo_get_plane_count(bo);
  std::vector<int32_t> strides =
      VideoFrame::ComputeStrides(PIXEL_FORMAT_NV12, coded_size);
  for (int i = 0; i < bo_num_planes; ++i) {
    uint32_t s = base::bits::AlignUpDeprecatedDoNotUse(strides[i], 64);
    EXPECT_EQ(s, gbm_bo_get_stride_for_plane(bo, i));
  }

  gbm_bo_destroy(bo);
  gbm_device_destroy(gbm);

  ASSERT_TRUE(OUTPUT_queue->Streamoff());
  ASSERT_TRUE(CAPTURE_queue->Streamoff());
  ASSERT_TRUE(OUTPUT_queue->DeallocateBuffers());
  ASSERT_TRUE(CAPTURE_queue->DeallocateBuffers());
}

// This test sets up a v4l2 device for the given video codec profiles,
// and resolution  (as per the test parameters). It then verifies that
// said metadata (e.g. width, height, number of planes, pitch) are the
// same as those we would allocate via minigbm.
TEST_P(V4L2MinigbmTest, AllocateAndCompareWithMinigbm) {
  const auto video_codec_profile = std::get<0>(GetParam());
  const gfx::Size resolution = std::get<1>(GetParam());

  scoped_refptr<V4L2Device> device(new V4L2Device());

  const auto fourcc_stateful =
      VideoCodecProfileToV4L2PixFmt(video_codec_profile, /*slice_based=*/false);
  const bool is_stateful =
      device->Open(V4L2Device::Type::kDecoder, fourcc_stateful);

  constexpr auto kCapsRequired = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
  struct v4l2_capability caps;
  if (device->Ioctl(VIDIOC_QUERYCAP, &caps) ||
      (caps.capabilities & kCapsRequired) != kCapsRequired) {
    GTEST_SKIP() << "Device doesn't support expected capabilities";
  }

  constexpr uint32_t desired_v4l2_pixel_formats[] = {V4L2_PIX_FMT_NV12,
                                                     V4L2_PIX_FMT_NV12M};
  std::vector<uint32_t> supported_v4l2_pixel_formats =
      EnumerateSupportedPixFmts(base::BindRepeating(&V4L2Device::Ioctl, device),
                                V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  int32_t chosen_v4l2_pixel_format = 0;
  for (const auto supported_v4l2_pixel_format : supported_v4l2_pixel_formats) {
    if (base::Contains(desired_v4l2_pixel_formats,
                       supported_v4l2_pixel_format)) {
      chosen_v4l2_pixel_format = supported_v4l2_pixel_format;
      break;
    }
  }
  ASSERT_GT(chosen_v4l2_pixel_format, 0);

  if (is_stateful) {
    TestStatefulDecoderAllocations(fourcc_stateful, device,
                                   chosen_v4l2_pixel_format, resolution);
  }
}

constexpr VideoCodecProfile kVideoCodecProfiles[] = {H264PROFILE_BASELINE};
constexpr gfx::Size kResolutions[] = {gfx::Size(127, 128), gfx::Size(128, 128),
                                      gfx::Size(323, 243), gfx::Size(640, 360),
                                      gfx::Size(1280, 720)};

INSTANTIATE_TEST_SUITE_P(
    ,
    V4L2MinigbmTest,
    ::testing::Combine(::testing::ValuesIn(kVideoCodecProfiles),
                       ::testing::ValuesIn(kResolutions)),
    V4L2MinigbmTest::PrintToStringParamName());

class MockVideoDecoderMixinClient : public VideoDecoderMixin::Client {
 public:
  MockVideoDecoderMixinClient() : weak_ptr_factory_(this) {}

  MOCK_METHOD(DmabufVideoFramePool*, GetVideoFramePool, (), (const, override));
  MOCK_METHOD(void, PrepareChangeResolution, (), (override));
  MOCK_METHOD(void, NotifyEstimatedMaxDecodeRequests, (int), (override));
  MOCK_METHOD(CroStatus::Or<ImageProcessor::PixelLayoutCandidate>,
              PickDecoderOutputFormat,
              (const std::vector<ImageProcessor::PixelLayoutCandidate>&,
               const gfx::Rect&,
               const gfx::Size&,
               std::optional<gfx::Size>,
               size_t,
               bool,
               bool,
               std::optional<DmabufVideoFramePool::CreateFrameCB>),
              (override));

  MOCK_METHOD(void, InitCallback, (DecoderStatus), ());

  base::WeakPtrFactory<MockVideoDecoderMixinClient> weak_ptr_factory_;
};

// Test fixture to use with V4L2StatefulVideoDecoder
class V4L2FlatVideoDecoderTest : public ::testing::Test {
 public:
  V4L2FlatVideoDecoderTest() = default;
  void SetUp() override {
    // Only run tests using V4L2StatefulVideoDecoder on platforms supporting the
    // V4L2 stateful decoder API.
    if (!IsV4L2DecoderStateful()) {
      GTEST_SKIP();
    }
  }
};

// Verifies that V4L2StatefulVideoDecoder::Initialize() fails when called with
// an unsupported codec profile.
TEST_F(V4L2FlatVideoDecoderTest, UnsupportedVideoCodec) {
  base::test::TaskEnvironment task_environment;
  MockVideoDecoderMixinClient mock_client;

  auto decoder = V4L2StatefulVideoDecoder::Create(
      std::make_unique<MockMediaLog>(),
      base::SequencedTaskRunner::GetCurrentDefault(),
      mock_client.weak_ptr_factory_.GetWeakPtr());

  const auto unsupported_config = TestVideoConfig::Normal(VideoCodec::kMPEG2);
  EXPECT_CALL(
      mock_client,
      InitCallback(DecoderStatus(DecoderStatus::Codes::kUnsupportedConfig)));
  static_cast<V4L2StatefulVideoDecoder*>(decoder.get())
      ->Initialize(unsupported_config, /*low_delay=*/false,
                   /*cdm_context=*/nullptr,
                   base::BindOnce(&MockVideoDecoderMixinClient::InitCallback,
                                  mock_client.weak_ptr_factory_.GetWeakPtr()),
                   /*output_cb=*/base::DoNothing(),
                   /*waiting_cb*/ base::DoNothing());
}

// Verifies that V4L2StatefulVideoDecoder::Initialize() fails after the limit of
// created instances exceeds the threshold.
TEST_F(V4L2FlatVideoDecoderTest, TooManyDecoderInstances) {
  base::test::TaskEnvironment task_environment;
  ::testing::NiceMock<MockVideoDecoderMixinClient> mock_client;
  const auto supported_config = TestVideoConfig::Normal(VideoCodec::kH264);

  const int kMaxNumOfInstances =
      V4L2StatefulVideoDecoder::GetMaxNumDecoderInstancesForTesting();

  ::testing::InSequence s;
  EXPECT_CALL(mock_client,
              InitCallback(DecoderStatus(DecoderStatus::Codes::kOk)))
      .Times(::testing::Exactly(kMaxNumOfInstances));

  std::vector<std::unique_ptr<VideoDecoderMixin>> decoders(kMaxNumOfInstances);
  for (auto& decoder : decoders) {
    decoder = V4L2StatefulVideoDecoder::Create(
        std::make_unique<MockMediaLog>(),
        base::SequencedTaskRunner::GetCurrentDefault(),
        mock_client.weak_ptr_factory_.GetWeakPtr());

    static_cast<V4L2StatefulVideoDecoder*>(decoder.get())
        ->Initialize(supported_config,
                     /*low_delay=*/false, /*cdm_context=*/nullptr,
                     base::BindOnce(&MockVideoDecoderMixinClient::InitCallback,
                                    mock_client.weak_ptr_factory_.GetWeakPtr()),
                     /*output_cb=*/base::DoNothing(),
                     /*waiting_cb*/ base::DoNothing());
  }
  testing::Mock::VerifyAndClearExpectations(&mock_client);

  // Next one fails:
  EXPECT_CALL(
      mock_client,
      InitCallback(DecoderStatus(DecoderStatus::Codes::kTooManyDecoders)));
  auto decoder = V4L2StatefulVideoDecoder::Create(
      std::make_unique<MockMediaLog>(),
      base::SequencedTaskRunner::GetCurrentDefault(),
      mock_client.weak_ptr_factory_.GetWeakPtr());
  static_cast<V4L2StatefulVideoDecoder*>(decoder.get())
      ->Initialize(supported_config,
                   /*low_delay=*/false, /*cdm_context=*/nullptr,
                   base::BindOnce(&MockVideoDecoderMixinClient::InitCallback,
                                  mock_client.weak_ptr_factory_.GetWeakPtr()),
                   /*output_cb=*/base::DoNothing(),
                   /*waiting_cb*/ base::DoNothing());
}

}  // namespace media

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
  {}

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
