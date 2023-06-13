// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>

#include "media/mojo/services/video_encoder_metrics_provider.h"

#include "base/run_loop.h"
#include "base/test/test_message_loop.h"
#include "components/ukm/test_ukm_recorder.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using UkmEntry = ukm::builders::Media_VideoEncoderMetrics;

using ::testing::TestWithParam;
using ::testing::ValuesIn;

namespace media {
namespace {
constexpr char kTestURL[] = "https://test.google.com/";

std::tuple<std::unique_ptr<ukm::TestAutoSetUkmRecorder>,
           mojo::Remote<mojom::VideoEncoderMetricsProvider>>
Create(const std::string& url) {
  auto test_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  ukm::SourceId source_id = test_recorder->GetNewSourceID();
  test_recorder->UpdateSourceURL(source_id, GURL(url));
  mojo::Remote<mojom::VideoEncoderMetricsProvider> provider;
  VideoEncoderMetricsProvider::Create(source_id,
                                      provider.BindNewPipeAndPassReceiver());
  return {std::move(test_recorder), std::move(provider)};
}
}  // namespace

class VideoEncoderMetricsProviderTest
    : public TestWithParam<testing::tuple<mojom::VideoEncoderUseCase,
                                          VideoCodecProfile,
                                          gfx::Size,
                                          bool,
                                          SVCScalabilityMode>> {
 public:
  VideoEncoderMetricsProviderTest() = default;
  ~VideoEncoderMetricsProviderTest() override = default;

 protected:
  base::TestMessageLoop message_loop_;
};

TEST_F(VideoEncoderMetricsProviderTest, Create_NoUKMReport) {
  auto [test_recorder, provider] = Create(kTestURL);
  provider.reset();
  base::RunLoop().RunUntilIdle();
  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_TRUE(entries.empty());
}

#define EXPECT_UKM(name, value) \
  test_recorder->ExpectEntryMetric(entry, name, value)

TEST_F(VideoEncoderMetricsProviderTest, CreateAndInitialize_ReportUKM) {
  auto [test_recorder, provider] = Create(kTestURL);
  constexpr auto kEncoderUseCase = mojom::VideoEncoderUseCase::kWebRTC;
  constexpr auto kCodecProfile = VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1200, 700);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;
  provider->Initialize(kEncoderUseCase, kCodecProfile, kEncodeSize,
                       kIsHardwareEncoder, kSVCMode);
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0];
  EXPECT_UKM(UkmEntry::kHeightName, kEncodeSize.height());
  EXPECT_UKM(UkmEntry::kIsHardwareName, kIsHardwareEncoder);
  EXPECT_UKM(UkmEntry::kNumEncodedFramesName, 0);
  EXPECT_UKM(UkmEntry::kProfileName, kCodecProfile);
  EXPECT_UKM(UkmEntry::kStatusName,
             static_cast<int64_t>(EncoderStatus::Codes::kOk));
  EXPECT_UKM(UkmEntry::kSVCModeName, static_cast<int64_t>(kSVCMode));
  EXPECT_UKM(UkmEntry::kUseCaseName, static_cast<int64_t>(kEncoderUseCase));
  EXPECT_UKM(UkmEntry::kWidthName, kEncodeSize.width());
}

TEST_F(
    VideoEncoderMetricsProviderTest,
    CreateAndInitializeAndSetSmallNumberEncodedFrameCount_ReportUKMWithOneBucket) {
  auto [test_recorder, provider] = Create(kTestURL);
  constexpr auto kEncoderUseCase = mojom::VideoEncoderUseCase::kWebRTC;
  constexpr auto kCodecProfile = VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1200, 700);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;
  provider->Initialize(kEncoderUseCase, kCodecProfile, kEncodeSize,
                       kIsHardwareEncoder, kSVCMode);
  provider->SetEncodedFrameCount(10);
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0];
  EXPECT_UKM(UkmEntry::kHeightName, kEncodeSize.height());
  EXPECT_UKM(UkmEntry::kIsHardwareName, kIsHardwareEncoder);
  EXPECT_UKM(UkmEntry::kNumEncodedFramesName, 1u);
  EXPECT_UKM(UkmEntry::kProfileName, kCodecProfile);
  EXPECT_UKM(UkmEntry::kStatusName,
             static_cast<int64_t>(EncoderStatus::Codes::kOk));
  EXPECT_UKM(UkmEntry::kSVCModeName, static_cast<int64_t>(kSVCMode));
  EXPECT_UKM(UkmEntry::kUseCaseName, static_cast<int64_t>(kEncoderUseCase));
  EXPECT_UKM(UkmEntry::kWidthName, kEncodeSize.width());
}

TEST_P(VideoEncoderMetricsProviderTest,
       CreateAndInitializeAndSetEncodedFrameCount_ReportUKM) {
  auto [test_recorder, provider] = Create(kTestURL);
  auto encoder_use_case = std::get<0>(GetParam());
  auto codec_profile = std::get<1>(GetParam());
  auto encode_size = std::get<2>(GetParam());
  auto is_hardware_encoder = std::get<3>(GetParam());
  auto svc_mode = std::get<4>(GetParam());
  constexpr uint64_t kNumEncodedFrames = 100;
  provider->Initialize(encoder_use_case, codec_profile, encode_size,
                       is_hardware_encoder, svc_mode);
  provider->SetEncodedFrameCount(kNumEncodedFrames);
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0];
  const uint64_t expected_height = encode_size.height() / 100 * 100;
  const uint64_t expected_width = encode_size.width() / 100 * 100;
  EXPECT_UKM(UkmEntry::kHeightName, expected_height);
  EXPECT_UKM(UkmEntry::kIsHardwareName, is_hardware_encoder);
  EXPECT_UKM(UkmEntry::kNumEncodedFramesName, kNumEncodedFrames);
  EXPECT_UKM(UkmEntry::kProfileName, codec_profile);
  EXPECT_UKM(UkmEntry::kStatusName,
             static_cast<int64_t>(EncoderStatus::Codes::kOk));
  EXPECT_UKM(UkmEntry::kSVCModeName, static_cast<int64_t>(svc_mode));
  EXPECT_UKM(UkmEntry::kUseCaseName, static_cast<int64_t>(encoder_use_case));
  EXPECT_UKM(UkmEntry::kWidthName, expected_width);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    VideoEncoderMetricsProviderTest,
    ::testing::Combine(ValuesIn({
                           mojom::VideoEncoderUseCase::kCastMirroring,
                           mojom::VideoEncoderUseCase::kMediaRecorder,
                           mojom::VideoEncoderUseCase::kWebCodecs,
                           mojom::VideoEncoderUseCase::kWebRTC,
                       }),
                       ValuesIn({
                           H264PROFILE_MAIN,
                           VP8PROFILE_ANY,
                           VP9PROFILE_MIN,
                           AV1PROFILE_PROFILE_HIGH,
                       }),
                       ValuesIn({
                           gfx::Size(640, 360),
                           gfx::Size(1280, 720),
                       }),
                       ::testing::Bool(),
                       ValuesIn({
                           SVCScalabilityMode::kL1T1,
                           SVCScalabilityMode::kL3T3Key,
                       })));

TEST_F(VideoEncoderMetricsProviderTest,
       InitializeWithVerfiyLargeResoloution_ReportCappedResolutionUKM) {
  auto [test_recorder, provider] = Create(kTestURL);
  constexpr auto kEncoderUseCase = mojom::VideoEncoderUseCase::kWebRTC;
  constexpr auto kCodecProfile = VP9PROFILE_PROFILE0;
  constexpr gfx::Size k16kEncodeSize(15360, 8640);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;
  constexpr uint64_t kNumEncodedFrames = 100;
  provider->Initialize(kEncoderUseCase, kCodecProfile, k16kEncodeSize,
                       kIsHardwareEncoder, kSVCMode);
  provider->SetEncodedFrameCount(kNumEncodedFrames);
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0];
  constexpr uint64_t kHeight = 8200;
  constexpr uint64_t kWidth = 8200;
  EXPECT_UKM(UkmEntry::kHeightName, kHeight);
  EXPECT_UKM(UkmEntry::kIsHardwareName, kIsHardwareEncoder);
  EXPECT_UKM(UkmEntry::kNumEncodedFramesName, kNumEncodedFrames);
  EXPECT_UKM(UkmEntry::kProfileName, kCodecProfile);
  EXPECT_UKM(UkmEntry::kStatusName,
             static_cast<int64_t>(EncoderStatus::Codes::kOk));
  EXPECT_UKM(UkmEntry::kSVCModeName, static_cast<int64_t>(kSVCMode));
  EXPECT_UKM(UkmEntry::kUseCaseName, static_cast<int64_t>(kEncoderUseCase));
  EXPECT_UKM(UkmEntry::kWidthName, kWidth);
}

TEST_F(VideoEncoderMetricsProviderTest,
       CallSetEncodedFrameCounts_ReportUKMWithTheLastEncodedFrameCount) {
  auto [test_recorder, provider] = Create(kTestURL);
  constexpr auto kEncoderUseCase = mojom::VideoEncoderUseCase::kWebRTC;
  constexpr auto kCodecProfile = VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1920, 1080);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;
  provider->Initialize(kEncoderUseCase, kCodecProfile, kEncodeSize,
                       kIsHardwareEncoder, kSVCMode);
  provider->SetEncodedFrameCount(100);
  provider->SetEncodedFrameCount(200);
  provider->SetEncodedFrameCount(300);
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0];
  constexpr uint64_t kHeight = kEncodeSize.height() / 100 * 100;
  constexpr uint64_t kWidth = kEncodeSize.width() / 100 * 100;
  EXPECT_UKM(UkmEntry::kHeightName, kHeight);
  EXPECT_UKM(UkmEntry::kIsHardwareName, kIsHardwareEncoder);
  EXPECT_UKM(UkmEntry::kNumEncodedFramesName, 300);
  EXPECT_UKM(UkmEntry::kProfileName, kCodecProfile);
  EXPECT_UKM(UkmEntry::kStatusName,
             static_cast<int64_t>(EncoderStatus::Codes::kOk));
  EXPECT_UKM(UkmEntry::kSVCModeName, static_cast<int64_t>(kSVCMode));
  EXPECT_UKM(UkmEntry::kUseCaseName, static_cast<int64_t>(kEncoderUseCase));
  EXPECT_UKM(UkmEntry::kWidthName, kWidth);
}

TEST_F(VideoEncoderMetricsProviderTest,
       CreateAndInitializeAndCallSetErrors_ReportUKMWithTheFirstError) {
  auto [test_recorder, provider] = Create(kTestURL);
  constexpr auto kEncoderUseCase = mojom::VideoEncoderUseCase::kWebRTC;
  constexpr auto kCodecProfile = VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1920, 1080);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;
  provider->Initialize(kEncoderUseCase, kCodecProfile, kEncodeSize,
                       kIsHardwareEncoder, kSVCMode);
  provider->SetError({EncoderStatus::Codes::kEncoderMojoConnectionError,
                      "mojo connection is disclosed"});
  provider->SetError(
      {EncoderStatus::Codes::kEncoderFailedEncode, "Encoder failed"});
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0];
  constexpr uint64_t kHeight = kEncodeSize.height() / 100 * 100;
  constexpr uint64_t kWidth = kEncodeSize.width() / 100 * 100;
  EXPECT_UKM(UkmEntry::kHeightName, kHeight);
  EXPECT_UKM(UkmEntry::kIsHardwareName, kIsHardwareEncoder);
  EXPECT_UKM(UkmEntry::kNumEncodedFramesName, 0u);
  EXPECT_UKM(UkmEntry::kProfileName, kCodecProfile);
  EXPECT_UKM(
      UkmEntry::kStatusName,
      static_cast<int64_t>(EncoderStatus::Codes::kEncoderMojoConnectionError));
  EXPECT_UKM(UkmEntry::kSVCModeName, static_cast<int64_t>(kSVCMode));
  EXPECT_UKM(UkmEntry::kUseCaseName, static_cast<int64_t>(kEncoderUseCase));
  EXPECT_UKM(UkmEntry::kWidthName, kWidth);
}

TEST_F(VideoEncoderMetricsProviderTest,
       CallErrorAndNoCallSetEncodedFramesCount_ReportUKMWithTheFirstError) {
  auto [test_recorder, provider] = Create(kTestURL);
  constexpr auto kEncoderUseCase = mojom::VideoEncoderUseCase::kWebRTC;
  constexpr auto kCodecProfile = VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1920, 1080);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;
  provider->Initialize(kEncoderUseCase, kCodecProfile, kEncodeSize,
                       kIsHardwareEncoder, kSVCMode);
  provider->SetError({EncoderStatus::Codes::kEncoderMojoConnectionError,
                      "mojo connection is disclosed"});
  provider->SetError(
      {EncoderStatus::Codes::kEncoderFailedEncode, "Encoder failed"});
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0];
  constexpr uint64_t kHeight = kEncodeSize.height() / 100 * 100;
  constexpr uint64_t kWidth = kEncodeSize.width() / 100 * 100;
  EXPECT_UKM(UkmEntry::kHeightName, kHeight);
  EXPECT_UKM(UkmEntry::kIsHardwareName, kIsHardwareEncoder);
  EXPECT_UKM(UkmEntry::kNumEncodedFramesName, 0);
  EXPECT_UKM(UkmEntry::kProfileName, kCodecProfile);
  EXPECT_UKM(
      UkmEntry::kStatusName,
      static_cast<int64_t>(EncoderStatus::Codes::kEncoderMojoConnectionError));
  EXPECT_UKM(UkmEntry::kSVCModeName, static_cast<int64_t>(kSVCMode));
  EXPECT_UKM(UkmEntry::kUseCaseName, static_cast<int64_t>(kEncoderUseCase));
  EXPECT_UKM(UkmEntry::kWidthName, kWidth);
}

TEST_F(
    VideoEncoderMetricsProviderTest,
    CallSetEncodedFrameCountsAndSetError_ReportUKMWithTheFirstErrorAndTheLastEncodedFrameCount) {
  auto [test_recorder, provider] = Create(kTestURL);
  constexpr auto kEncoderUseCase = mojom::VideoEncoderUseCase::kWebRTC;
  constexpr auto kCodecProfile = VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1920, 1080);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;
  provider->Initialize(kEncoderUseCase, kCodecProfile, kEncodeSize,
                       kIsHardwareEncoder, kSVCMode);
  provider->SetEncodedFrameCount(100);
  provider->SetEncodedFrameCount(200);
  provider->SetEncodedFrameCount(300);
  provider->SetError(
      {EncoderStatus::Codes::kEncoderFailedEncode, "Encoder failed"});
  provider->SetError(
      {EncoderStatus::Codes::kEncoderIllegalState, "Encoder illegal state"});
  provider->SetEncodedFrameCount(400);
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0];
  constexpr uint64_t kHeight = kEncodeSize.height() / 100 * 100;
  constexpr uint64_t kWidth = kEncodeSize.width() / 100 * 100;
  EXPECT_UKM(UkmEntry::kHeightName, kHeight);
  EXPECT_UKM(UkmEntry::kIsHardwareName, kIsHardwareEncoder);
  EXPECT_UKM(UkmEntry::kNumEncodedFramesName, 400);
  EXPECT_UKM(UkmEntry::kProfileName, kCodecProfile);
  EXPECT_UKM(UkmEntry::kStatusName,
             static_cast<int64_t>(EncoderStatus::Codes::kEncoderFailedEncode));
  EXPECT_UKM(UkmEntry::kSVCModeName, static_cast<int64_t>(kSVCMode));
  EXPECT_UKM(UkmEntry::kUseCaseName, static_cast<int64_t>(kEncoderUseCase));
  EXPECT_UKM(UkmEntry::kWidthName, kWidth);
}

TEST_F(VideoEncoderMetricsProviderTest,
       CreateAndTwoInitializeAndSetEncodedFrameCounts_ReportTwoUKMs) {
  const struct {
    mojom::VideoEncoderUseCase use_case;
    VideoCodecProfile profile;
    gfx::Size size;
    bool is_hardware;
    SVCScalabilityMode svc_mode;
    EncoderStatus::Codes status;
    uint64_t num_encoded_frames;
  } kMetricsCases[] = {
      {
          mojom::VideoEncoderUseCase::kWebRTC,
          VP9PROFILE_PROFILE0,
          gfx::Size(600, 300),
          true,
          SVCScalabilityMode::kL2T3Key,
          EncoderStatus::Codes::kEncoderIllegalState,
          100,
      },
      {
          mojom::VideoEncoderUseCase::kMediaRecorder,
          H264PROFILE_HIGH,
          gfx::Size(1200, 700),
          /*is_hardware=*/true,
          SVCScalabilityMode::kL2T3Key,
          EncoderStatus::Codes::kOk,
          300,
      },
  };
  auto [test_recorder, provider] = Create(kTestURL);
  for (const auto& metrics : kMetricsCases) {
    provider->Initialize(metrics.use_case, metrics.profile, metrics.size,
                         metrics.is_hardware, metrics.svc_mode);
    provider->SetEncodedFrameCount(metrics.num_encoded_frames);
    if (metrics.status != EncoderStatus::Codes::kOk) {
      provider->SetError(metrics.status);
    }
  }
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(std::size(kMetricsCases), entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto* entry = entries[i];
    const auto& metrics = kMetricsCases[i];
    EXPECT_UKM(UkmEntry::kHeightName, metrics.size.height());
    EXPECT_UKM(UkmEntry::kIsHardwareName, metrics.is_hardware);
    EXPECT_UKM(UkmEntry::kNumEncodedFramesName, metrics.num_encoded_frames);
    EXPECT_UKM(UkmEntry::kProfileName, metrics.profile);
    EXPECT_UKM(UkmEntry::kStatusName, static_cast<int64_t>(metrics.status));
    EXPECT_UKM(UkmEntry::kSVCModeName, static_cast<int64_t>(metrics.svc_mode));
    EXPECT_UKM(UkmEntry::kUseCaseName, static_cast<int64_t>(metrics.use_case));
    EXPECT_UKM(UkmEntry::kWidthName, metrics.size.width());
  }
}

#undef EXPECT_UKM
}  // namespace media
