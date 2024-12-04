// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <memory>

#include "media/mojo/services/mojo_video_encoder_metrics_provider_service.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
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
  MojoVideoEncoderMetricsProviderService::Create(
      source_id, provider.BindNewPipeAndPassReceiver());
  return {std::move(test_recorder), std::move(provider)};
}
}  // namespace

class MojoVideoEncoderMetricsProviderServiceTest
    : public TestWithParam<testing::tuple<uint64_t,
                                          mojom::VideoEncoderUseCase,
                                          VideoCodecProfile,
                                          gfx::Size,
                                          bool,
                                          SVCScalabilityMode,
                                          EncoderStatus::Codes>> {
 public:
  MojoVideoEncoderMetricsProviderServiceTest() = default;
  ~MojoVideoEncoderMetricsProviderServiceTest() override = default;

 protected:
  base::TestMessageLoop message_loop_;
};

TEST_F(MojoVideoEncoderMetricsProviderServiceTest, Create_NoUKMReport) {
  auto [test_recorder, provider] = Create(kTestURL);
  provider.reset();
  base::RunLoop().RunUntilIdle();
  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_TRUE(entries.empty());
}

#define EXPECT_UKM(name, value) \
  test_recorder->ExpectEntryMetric(entry, name, value)

TEST_F(MojoVideoEncoderMetricsProviderServiceTest,
       CreateAndInitialize_ReportUKM) {
  auto [test_recorder, provider] = Create(kTestURL);
  constexpr uint64_t kEncoderId = 0;
  constexpr auto kEncoderUseCase = mojom::VideoEncoderUseCase::kWebRTC;
  constexpr auto kCodecProfile = VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1200, 700);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;
  provider->Initialize(kEncoderId, kEncoderUseCase, kCodecProfile, kEncodeSize,
                       kIsHardwareEncoder, kSVCMode);
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0].get();
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

TEST_F(MojoVideoEncoderMetricsProviderServiceTest,
       CreateAndInitializeAndComplete_ReportUKM) {
  auto [test_recorder, provider] = Create(kTestURL);
  constexpr uint64_t kEncoderId = 0;
  constexpr auto kEncoderUseCase = mojom::VideoEncoderUseCase::kWebRTC;
  constexpr auto kCodecProfile = VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1200, 700);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;
  provider->Initialize(kEncoderId, kEncoderUseCase, kCodecProfile, kEncodeSize,
                       kIsHardwareEncoder, kSVCMode);
  provider->Complete(kEncoderId);
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0].get();
  EXPECT_UKM(UkmEntry::kHeightName, kEncodeSize.height());
  EXPECT_UKM(UkmEntry::kIsHardwareName, kIsHardwareEncoder);
  EXPECT_UKM(UkmEntry::kNumEncodedFramesName, 0);
  EXPECT_UKM(UkmEntry::kProfileName, kCodecProfile);
  EXPECT_UKM(UkmEntry::kStatusName,
             static_cast<int64_t>(EncoderStatus::Codes::kOk));
  EXPECT_UKM(UkmEntry::kSVCModeName, static_cast<int64_t>(kSVCMode));
  EXPECT_UKM(UkmEntry::kUseCaseName, static_cast<int64_t>(kEncoderUseCase));
  EXPECT_UKM(UkmEntry::kWidthName, kEncodeSize.width());
  provider.reset();
}

TEST_F(
    MojoVideoEncoderMetricsProviderServiceTest,
    CreateAndInitializeAndSetSmallNumberEncodedFrameCount_ReportUKMWithOneBucket) {
  auto [test_recorder, provider] = Create(kTestURL);
  constexpr uint64_t kEncoderId = 0;
  constexpr auto kEncoderUseCase = mojom::VideoEncoderUseCase::kWebRTC;
  constexpr auto kCodecProfile = VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1200, 700);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;
  provider->Initialize(kEncoderId, kEncoderUseCase, kCodecProfile, kEncodeSize,
                       kIsHardwareEncoder, kSVCMode);
  provider->SetEncodedFrameCount(kEncoderId, 10);
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0].get();
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

TEST_P(MojoVideoEncoderMetricsProviderServiceTest,
       CreateAndInitializeAndSetEncodedFrameCount_ReportUKM) {
  auto [test_recorder, provider] = Create(kTestURL);
  auto encoder_id = std::get<0>(GetParam());
  auto encoder_use_case = std::get<1>(GetParam());
  auto codec_profile = std::get<2>(GetParam());
  auto encode_size = std::get<3>(GetParam());
  auto is_hardware_encoder = std::get<4>(GetParam());
  auto svc_mode = std::get<5>(GetParam());
  media::EncoderStatus encoder_status(std::get<6>(GetParam()));
  constexpr uint64_t kNumEncodedFrames = 100;
  provider->Initialize(encoder_id, encoder_use_case, codec_profile, encode_size,
                       is_hardware_encoder, svc_mode);
  provider->SetEncodedFrameCount(encoder_id, kNumEncodedFrames);
  if (!encoder_status.is_ok()) {
    provider->SetError(encoder_id, encoder_status);
  }
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0].get();
  const uint64_t expected_height = encode_size.height() / 100 * 100;
  const uint64_t expected_width = encode_size.width() / 100 * 100;
  EXPECT_UKM(UkmEntry::kHeightName, expected_height);
  EXPECT_UKM(UkmEntry::kIsHardwareName, is_hardware_encoder);
  EXPECT_UKM(UkmEntry::kNumEncodedFramesName, kNumEncodedFrames);
  EXPECT_UKM(UkmEntry::kProfileName, codec_profile);
  EXPECT_UKM(UkmEntry::kSVCModeName, static_cast<int64_t>(svc_mode));
  EXPECT_UKM(UkmEntry::kUseCaseName, static_cast<int64_t>(encoder_use_case));
  EXPECT_UKM(UkmEntry::kWidthName, expected_width);
  EXPECT_UKM(UkmEntry::kStatusName,
             static_cast<int64_t>(encoder_status.code()));
}

TEST_P(MojoVideoEncoderMetricsProviderServiceTest,
       CreateAndInitializeAndSetEncodedFrameCount_ReportUMA) {
  base::HistogramTester histogram_tester;
  auto [test_recorder, provider] = Create(kTestURL);
  auto encoder_id = std::get<0>(GetParam());
  auto encoder_use_case = std::get<1>(GetParam());
  auto codec_profile = std::get<2>(GetParam());
  auto encode_size = std::get<3>(GetParam());
  auto is_hardware_encoder = std::get<4>(GetParam());
  auto svc_mode = std::get<5>(GetParam());
  media::EncoderStatus encoder_status(std::get<6>(GetParam()));
  constexpr uint64_t kNumEncodedFrames = 100;
  provider->Initialize(encoder_id, encoder_use_case, codec_profile, encode_size,
                       is_hardware_encoder, svc_mode);
  provider->SetEncodedFrameCount(encoder_id, kNumEncodedFrames);
  if (!encoder_status.is_ok()) {
    provider->SetError(encoder_id, encoder_status);
  }
  provider.reset();
  base::RunLoop().RunUntilIdle();

  std::string uma_prefix = "Media.VideoEncoder.";
  switch (encoder_use_case) {
    case mojom::VideoEncoderUseCase::kCastMirroring:
      uma_prefix += "CastMirroring.";
      break;
    case mojom::VideoEncoderUseCase::kMediaRecorder:
      uma_prefix += "MediaRecorder.";
      break;
    case mojom::VideoEncoderUseCase::kWebCodecs:
      uma_prefix += "WebCodecs.";
      break;
    case mojom::VideoEncoderUseCase::kWebRTC:
      uma_prefix += "WebRTC.";
      break;
  }
  uma_prefix += is_hardware_encoder ? "HW." : "SW.";

#define EXPECT_UMA(name, value)                                           \
  do {                                                                    \
    histogram_tester.ExpectUniqueSample(base::StrCat({uma_prefix, name}), \
                                        value, 1);                        \
  } while (0)

  EXPECT_UMA("Profile", codec_profile);
  EXPECT_UMA("SVC", svc_mode);
  EXPECT_UMA("Width", encode_size.width());
  EXPECT_UMA("Height", encode_size.height());
  EXPECT_UMA("Area", encode_size.GetArea() / 100);
  EXPECT_UMA("Status", encoder_status.code());

#undef EXPECT_UMA
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MojoVideoEncoderMetricsProviderServiceTest,
    ::testing::Combine(ValuesIn(std::vector<uint64_t>{12ul, 100ul}),
                       ValuesIn({
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
                       }),
                       ValuesIn({
                           EncoderStatus::Codes::kOk,
                           EncoderStatus::Codes::kEncoderFailedEncode,
                       })));

TEST_F(MojoVideoEncoderMetricsProviderServiceTest,
       InitializeWithVerfiyLargeResoloution_ReportCappedResolutionUKM) {
  auto [test_recorder, provider] = Create(kTestURL);
  constexpr uint64_t kEncoderId = 0;
  constexpr auto kEncoderUseCase = mojom::VideoEncoderUseCase::kWebRTC;
  constexpr auto kCodecProfile = VP9PROFILE_PROFILE0;
  constexpr gfx::Size k16kEncodeSize(15360, 8640);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;
  constexpr uint64_t kNumEncodedFrames = 100;
  provider->Initialize(kEncoderId, kEncoderUseCase, kCodecProfile,
                       k16kEncodeSize, kIsHardwareEncoder, kSVCMode);
  provider->SetEncodedFrameCount(kEncoderId, kNumEncodedFrames);
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0].get();
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

TEST_F(MojoVideoEncoderMetricsProviderServiceTest,
       CallSetEncodedFrameCounts_ReportUKMWithTheLastEncodedFrameCount) {
  auto [test_recorder, provider] = Create(kTestURL);
  constexpr uint64_t kEncoderId = 0;
  constexpr auto kEncoderUseCase = mojom::VideoEncoderUseCase::kWebRTC;
  constexpr auto kCodecProfile = VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1920, 1080);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;
  provider->Initialize(kEncoderId, kEncoderUseCase, kCodecProfile, kEncodeSize,
                       kIsHardwareEncoder, kSVCMode);
  provider->SetEncodedFrameCount(kEncoderId, 100);
  provider->SetEncodedFrameCount(kEncoderId, 200);
  provider->SetEncodedFrameCount(kEncoderId, 300);
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0].get();
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

TEST_F(MojoVideoEncoderMetricsProviderServiceTest,
       CreateAndInitializeAndCallSetErrors_ReportUKMWithTheFirstError) {
  auto [test_recorder, provider] = Create(kTestURL);
  constexpr uint64_t kEncoderId = 0;
  constexpr auto kEncoderUseCase = mojom::VideoEncoderUseCase::kWebRTC;
  constexpr auto kCodecProfile = VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1920, 1080);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;
  provider->Initialize(kEncoderId, kEncoderUseCase, kCodecProfile, kEncodeSize,
                       kIsHardwareEncoder, kSVCMode);
  provider->SetError(kEncoderId,
                     {EncoderStatus::Codes::kEncoderMojoConnectionError,
                      "mojo connection is disclosed"});
  provider->SetError(kEncoderId, {EncoderStatus::Codes::kEncoderFailedEncode,
                                  "Encoder failed"});
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0].get();
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

TEST_F(MojoVideoEncoderMetricsProviderServiceTest,
       CallErrorAndNoCallSetEncodedFramesCount_ReportUKMWithTheFirstError) {
  auto [test_recorder, provider] = Create(kTestURL);
  constexpr uint64_t kEncoderId = 0;
  constexpr auto kEncoderUseCase = mojom::VideoEncoderUseCase::kWebRTC;
  constexpr auto kCodecProfile = VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1920, 1080);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;
  provider->Initialize(kEncoderId, kEncoderUseCase, kCodecProfile, kEncodeSize,
                       kIsHardwareEncoder, kSVCMode);
  provider->SetError(kEncoderId,
                     {EncoderStatus::Codes::kEncoderMojoConnectionError,
                      "mojo connection is disclosed"});
  provider->SetError(kEncoderId, {EncoderStatus::Codes::kEncoderFailedEncode,
                                  "Encoder failed"});
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0].get();
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
    MojoVideoEncoderMetricsProviderServiceTest,
    CallSetEncodedFrameCountsAndSetError_ReportUKMWithTheFirstErrorAndTheLastEncodedFrameCount) {
  auto [test_recorder, provider] = Create(kTestURL);
  constexpr uint64_t kEncoderId = 0;
  constexpr auto kEncoderUseCase = mojom::VideoEncoderUseCase::kWebRTC;
  constexpr auto kCodecProfile = VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1920, 1080);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;
  provider->Initialize(kEncoderId, kEncoderUseCase, kCodecProfile, kEncodeSize,
                       kIsHardwareEncoder, kSVCMode);
  provider->SetEncodedFrameCount(kEncoderId, 100);
  provider->SetEncodedFrameCount(kEncoderId, 200);
  provider->SetEncodedFrameCount(kEncoderId, 300);
  provider->SetError(kEncoderId, {EncoderStatus::Codes::kEncoderFailedEncode,
                                  "Encoder failed"});
  provider->SetError(kEncoderId, {EncoderStatus::Codes::kEncoderIllegalState,
                                  "Encoder illegal state"});
  provider->SetEncodedFrameCount(kEncoderId, 400);
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0].get();
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

TEST_F(MojoVideoEncoderMetricsProviderServiceTest,
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
  constexpr uint64_t kEncoderId = 0;
  auto [test_recorder, provider] = Create(kTestURL);
  for (const auto& metrics : kMetricsCases) {
    provider->Initialize(kEncoderId, metrics.use_case, metrics.profile,
                         metrics.size, metrics.is_hardware, metrics.svc_mode);
    provider->SetEncodedFrameCount(kEncoderId, metrics.num_encoded_frames);
    if (metrics.status != EncoderStatus::Codes::kOk) {
      provider->SetError(kEncoderId, metrics.status);
    }
  }
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(std::size(kMetricsCases), entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto* entry = entries[i].get();
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

TEST_F(MojoVideoEncoderMetricsProviderServiceTest, HandleTwoEncoders) {
  const struct {
    uint64_t encoder_id;
    mojom::VideoEncoderUseCase use_case;
    VideoCodecProfile profile;
    gfx::Size size;
    bool is_hardware;
    SVCScalabilityMode svc_mode;
    EncoderStatus::Codes status;
    uint64_t num_encoded_frames;
  } kMetricsCases[] = {
      {
          0,
          mojom::VideoEncoderUseCase::kWebRTC,
          VP9PROFILE_PROFILE0,
          gfx::Size(600, 300),
          true,
          SVCScalabilityMode::kL2T3Key,
          EncoderStatus::Codes::kEncoderIllegalState,
          100,
      },
      {
          1,
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
    provider->Initialize(metrics.encoder_id, metrics.use_case, metrics.profile,
                         metrics.size, metrics.is_hardware, metrics.svc_mode);
  }
  for (const auto& metrics : kMetricsCases) {
    provider->SetEncodedFrameCount(metrics.encoder_id,
                                   metrics.num_encoded_frames);
  }
  for (const auto& metrics : kMetricsCases) {
    if (metrics.status != EncoderStatus::Codes::kOk) {
      provider->SetError(metrics.encoder_id, metrics.status);
    }
  }
  for (const auto& metrics : kMetricsCases) {
    provider->Complete(metrics.encoder_id);
  }
  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(std::size(kMetricsCases), entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto* entry = entries[i].get();
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

TEST_F(MojoVideoEncoderMetricsProviderServiceTest, IgnoreUnknownEncoderIds) {
  auto [test_recorder, provider] = Create(kTestURL);
  constexpr uint64_t kKnownEncoderId = 123;
  constexpr uint64_t kUnknownEncoderId = 321;
  constexpr auto kEncoderUseCase = mojom::VideoEncoderUseCase::kWebRTC;
  constexpr auto kCodecProfile = VP9PROFILE_PROFILE0;
  constexpr gfx::Size kEncodeSize(1200, 700);
  constexpr bool kIsHardwareEncoder = true;
  constexpr auto kSVCMode = SVCScalabilityMode::kL1T3;
  provider->Initialize(kKnownEncoderId, kEncoderUseCase, kCodecProfile,
                       kEncodeSize, kIsHardwareEncoder, kSVCMode);
  provider->SetEncodedFrameCount(kUnknownEncoderId, 100);
  provider->SetError(kUnknownEncoderId,
                     EncoderStatus::Codes::kEncoderFailedEncode);
  provider->Complete(kUnknownEncoderId);

  provider->Complete(kKnownEncoderId);
  // This should be ignored as Complete() is already called.
  provider->SetError(kKnownEncoderId,
                     EncoderStatus::Codes::kEncoderFailedEncode);

  provider.reset();
  base::RunLoop().RunUntilIdle();

  const auto entries = test_recorder->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries[0].get();
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
#undef EXPECT_UKM
}  // namespace media
