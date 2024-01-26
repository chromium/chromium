// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_message_loop.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "media/mojo/services/media_metrics_provider.h"
#include "media/mojo/services/watch_time_recorder.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using UkmEntry = ukm::builders::Media_WebMediaPlayerState;

namespace media {

constexpr char kTestOrigin[] = "https://test.google.com/";

class MediaMetricsProviderTest : public testing::Test {
 public:
  MediaMetricsProviderTest() { ResetMetricRecorders(); }

  MediaMetricsProviderTest(const MediaMetricsProviderTest&) = delete;
  MediaMetricsProviderTest& operator=(const MediaMetricsProviderTest&) = delete;

  ~MediaMetricsProviderTest() override { base::RunLoop().RunUntilIdle(); }

  void Initialize(bool is_mse,
                  bool is_incognito,
                  bool is_top_frame,
                  const std::string& origin,
                  mojom::MediaURLScheme scheme,
                  mojom::MediaStreamType media_stream_type =
                      mojom::MediaStreamType::kNone) {
    source_id_ = test_recorder_->GetNewSourceID();
    test_recorder_->UpdateSourceURL(source_id_, GURL(origin));

    MediaMetricsProvider::Create(
        (is_incognito ? MediaMetricsProvider::BrowsingMode::kIncognito
                      : MediaMetricsProvider::BrowsingMode::kNormal),
        (is_top_frame ? MediaMetricsProvider::FrameStatus::kTopFrame
                      : MediaMetricsProvider::FrameStatus::kNotTopFrame),
        GetSourceId(), learning::FeatureValue(0),
        VideoDecodePerfHistory::SaveCallback(),
        MediaMetricsProvider::GetLearningSessionCallback(),
        base::BindRepeating(&MediaMetricsProviderTest::IsShuttingDown,
                            base::Unretained(this)),
        provider_.BindNewPipeAndPassReceiver());
    provider_->Initialize(is_mse, scheme, media_stream_type);
  }

  ukm::SourceId GetSourceId() { return source_id_; }

  MOCK_METHOD(bool, IsShuttingDown, ());

  void ResetMetricRecorders() {
    // Ensure cleared global before attempting to create a new TestUkmReporter.
    test_recorder_.reset();
    test_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

 protected:
  base::TestMessageLoop message_loop_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_recorder_;
  ukm::SourceId source_id_;
  mojo::Remote<mojom::MediaMetricsProvider> provider_;
};

#define EXPECT_UKM(name, value) \
  test_recorder_->ExpectEntryMetric(entry, name, value)
#define EXPECT_NO_UKM(name) \
  EXPECT_FALSE(test_recorder_->EntryHasMetric(entry, name))
#define EXPECT_HAS_UKM(name) \
  EXPECT_TRUE(test_recorder_->EntryHasMetric(entry, name));

TEST_F(MediaMetricsProviderTest, TestUkm) {
  Initialize(true, false, true, kTestOrigin, mojom::MediaURLScheme::kHttp);
  provider_.reset();
  base::RunLoop().RunUntilIdle();

  {
    const auto& entries =
        test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const ukm::mojom::UkmEntry* entry : entries) {
      test_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestOrigin));
      EXPECT_HAS_UKM(UkmEntry::kPlayerIDName);
      EXPECT_UKM(UkmEntry::kIsTopFrameName, true);
      EXPECT_UKM(UkmEntry::kIsEMEName, false);
      EXPECT_UKM(UkmEntry::kKeySystemName, 0);
      EXPECT_UKM(UkmEntry::kIsHardwareSecureName, false);
      EXPECT_UKM(UkmEntry::kIsMSEName, true);
      EXPECT_UKM(UkmEntry::kFinalPipelineStatusName, PIPELINE_OK);

      // This is an MSE playback so the URL scheme should not be set.
      EXPECT_NO_UKM(UkmEntry::kURLSchemeName);

      // This is an MSE playback so no container is available.
      EXPECT_NO_UKM(UkmEntry::kContainerNameName);

      EXPECT_NO_UKM(UkmEntry::kTimeToMetadataName);
      EXPECT_NO_UKM(UkmEntry::kTimeToFirstFrameName);
      EXPECT_NO_UKM(UkmEntry::kTimeToPlayReadyName);
    }
  }

  // Now try one with different values and optional parameters set.
  const std::string kTestOrigin2 = "https://test2.google.com/";
  const base::TimeDelta kMetadataTime = base::Seconds(1);
  const base::TimeDelta kFirstFrameTime = base::Seconds(2);
  const base::TimeDelta kPlayReadyTime = base::Seconds(3);

  ResetMetricRecorders();
  Initialize(false, false, false, kTestOrigin2, mojom::MediaURLScheme::kHttps);
  provider_->SetIsEME();
  provider_->SetKeySystem(kClearKeyKeySystem);
  provider_->SetHasWaitingForKey();
  provider_->SetIsHardwareSecure();
  provider_->SetAudioPipelineInfo(
      {false, false, AudioDecoderType::kMojo, EncryptionType::kClear});
  provider_->SetVideoPipelineInfo(
      {false, false, VideoDecoderType::kMojo, EncryptionType::kEncrypted});
  provider_->SetTimeToMetadata(kMetadataTime);
  provider_->SetTimeToFirstFrame(kFirstFrameTime);
  provider_->SetTimeToPlayReady(kPlayReadyTime);
  provider_->SetContainerName(
      container_names::MediaContainerName::kContainerMOV);
  provider_->OnError(PIPELINE_ERROR_DECODE);
  provider_.reset();
  base::RunLoop().RunUntilIdle();

  {
    const auto& entries =
        test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const ukm::mojom::UkmEntry* entry : entries) {
      test_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestOrigin2));
      EXPECT_HAS_UKM(UkmEntry::kPlayerIDName);
      EXPECT_UKM(UkmEntry::kIsTopFrameName, false);
      EXPECT_UKM(UkmEntry::kIsEMEName, true);
      EXPECT_UKM(UkmEntry::kKeySystemName, 1);
      EXPECT_UKM(UkmEntry::kHasWaitingForKeyName, true);
      EXPECT_UKM(UkmEntry::kIsHardwareSecureName, true);
      EXPECT_UKM(UkmEntry::kAudioEncryptionTypeName,
                 static_cast<int64_t>(EncryptionType::kClear));
      EXPECT_UKM(UkmEntry::kVideoEncryptionTypeName,
                 static_cast<int64_t>(EncryptionType::kEncrypted));
      EXPECT_UKM(UkmEntry::kIsMSEName, false);
      EXPECT_UKM(UkmEntry::kURLSchemeName,
                 static_cast<int64_t>(mojom::MediaURLScheme::kHttps));
      EXPECT_UKM(UkmEntry::kFinalPipelineStatusName, PIPELINE_ERROR_DECODE);
      EXPECT_UKM(UkmEntry::kTimeToMetadataName, kMetadataTime.InMilliseconds());
      EXPECT_UKM(UkmEntry::kTimeToFirstFrameName,
                 kFirstFrameTime.InMilliseconds());
      EXPECT_UKM(UkmEntry::kTimeToPlayReadyName,
                 kPlayReadyTime.InMilliseconds());
      EXPECT_UKM(UkmEntry::kContainerNameName,
                 base::to_underlying(
                     container_names::MediaContainerName::kContainerMOV));
    }
  }
}

TEST_F(MediaMetricsProviderTest, TestUkmMediaStream) {
  Initialize(true, false, true, kTestOrigin, mojom::MediaURLScheme::kMissing,
             mojom::MediaStreamType::kRemote);
  provider_.reset();
  base::RunLoop().RunUntilIdle();

  {
    const auto& entries =
        test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(0u, entries.size());
  }

  // Now try one with different values and optional parameters set.
  const std::string kTestOrigin2 = "https://test2.google.com/";
  const base::TimeDelta kMetadataTime = base::Seconds(1);
  const base::TimeDelta kFirstFrameTime = base::Seconds(2);
  const base::TimeDelta kPlayReadyTime = base::Seconds(3);

  ResetMetricRecorders();
  Initialize(false, false, false, kTestOrigin2, mojom::MediaURLScheme::kMissing,
             mojom::MediaStreamType::kLocalDeviceCapture);
  provider_->SetIsEME();
  provider_->SetTimeToMetadata(kMetadataTime);
  provider_->SetTimeToFirstFrame(kFirstFrameTime);
  provider_->SetTimeToPlayReady(kPlayReadyTime);
  provider_->SetContainerName(
      container_names::MediaContainerName::kContainerMOV);
  provider_->OnError(PIPELINE_ERROR_DECODE);
  provider_.reset();
  base::RunLoop().RunUntilIdle();

  {
    const auto& entries =
        test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(0u, entries.size());
  }
}

TEST_F(MediaMetricsProviderTest, TestPipelineUMA) {
  base::HistogramTester histogram_tester;
  Initialize(false, false, false, kTestOrigin, mojom::MediaURLScheme::kHttps);
  provider_->SetAudioPipelineInfo(
      {false, false, AudioDecoderType::kMojo, EncryptionType::kClear});
  provider_->SetVideoPipelineInfo(
      {false, false, VideoDecoderType::kMojo, EncryptionType::kEncrypted});
  provider_->SetHasAudio(AudioCodec::kVorbis);
  provider_->SetHasVideo(VideoCodec::kVP9);
  provider_->SetHasPlayed();
  provider_->SetHaveEnough();
  provider_.reset();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount("Media.PipelineStatus.AudioVideo.VP9.SW",
                                     PIPELINE_OK, 1);
  histogram_tester.ExpectBucketCount("Media.VideoDecoderFallback.VP9", false,
                                     1);
  histogram_tester.ExpectBucketCount("Media.HasEverPlayed", true, 1);
}

TEST_F(MediaMetricsProviderTest, TestPipelineUMAMediaStream) {
  base::HistogramTester histogram_tester;
  Initialize(false, false, false, kTestOrigin, mojom::MediaURLScheme::kHttps,
             mojom::MediaStreamType::kRemote);
  provider_->SetAudioPipelineInfo(
      {false, false, AudioDecoderType::kMojo, EncryptionType::kClear});
  provider_->SetVideoPipelineInfo(
      {false, false, VideoDecoderType::kMojo, EncryptionType::kEncrypted});
  provider_->SetHasAudio(AudioCodec::kVorbis);
  provider_->SetHasVideo(VideoCodec::kVP9);
  provider_->SetHasPlayed();
  provider_->SetHaveEnough();
  provider_.reset();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount("Media.PipelineStatus.AudioVideo.VP9.SW",
                                     PIPELINE_OK, 0);
  histogram_tester.ExpectBucketCount("Media.VideoDecoderFallback.VP9", false,
                                     0);
  histogram_tester.ExpectBucketCount("Media.HasEverPlayed", true, 0);
}

TEST_F(MediaMetricsProviderTest, TestPipelineUMANoAudioWithEme) {
  base::HistogramTester histogram_tester;
  Initialize(false, false, false, kTestOrigin, mojom::MediaURLScheme::kHttps);
  provider_->SetIsEME();
  provider_->SetVideoPipelineInfo(
      {true, true, VideoDecoderType::kMojo, EncryptionType::kEncrypted});
  provider_->SetHasVideo(VideoCodec::kAV1);
  provider_->SetHasPlayed();
  provider_->SetHaveEnough();
  provider_.reset();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount("Media.PipelineStatus.VideoOnly",
                                     PIPELINE_OK, 1);
  histogram_tester.ExpectBucketCount("Media.VideoDecoderFallback.AV1", false,
                                     1);
  histogram_tester.ExpectBucketCount("Media.HasEverPlayed", true, 1);
  histogram_tester.ExpectBucketCount("Media.EME.IsIncognito", false, 1);
}

TEST_F(MediaMetricsProviderTest, TestPipelineUMADecoderFallback) {
  base::HistogramTester histogram_tester;
  Initialize(false, false, false, kTestOrigin, mojom::MediaURLScheme::kHttps);
  provider_->SetIsEME();
  provider_->SetAudioPipelineInfo(
      {false, false, AudioDecoderType::kMojo, EncryptionType::kClear});
  provider_->SetVideoPipelineInfo(
      {true, false, VideoDecoderType::kD3D11, EncryptionType::kEncrypted});
  provider_->SetHasVideo(VideoCodec::kVP9);
  provider_->SetHasAudio(AudioCodec::kVorbis);
  provider_->SetHasPlayed();
  provider_->SetHaveEnough();
  provider_->SetVideoPipelineInfo({true, false, VideoDecoderType::kFFmpeg});
  provider_.reset();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount("Media.PipelineStatus.AudioVideo.VP9.HW",
                                     PIPELINE_OK, 1);
  histogram_tester.ExpectBucketCount("Media.VideoDecoderFallback.VP9", true, 1);
  histogram_tester.ExpectBucketCount("Media.HasEverPlayed", true, 1);
}

TEST_F(MediaMetricsProviderTest, TestPipelineUMARendererType) {
  base::HistogramTester histogram_tester;
  Initialize(false, false, false, kTestOrigin, mojom::MediaURLScheme::kHttps);
  provider_->SetIsEME();
  provider_->SetRendererType(RendererType::kMediaFoundation);
  provider_->SetHasVideo(VideoCodec::kVP9);
  provider_->SetHasAudio(AudioCodec::kVorbis);
  provider_->SetHasPlayed();
  provider_->SetHaveEnough();
  provider_.reset();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "Media.PipelineStatus.AudioVideo.VP9.MediaFoundationRenderer",
      PIPELINE_OK, 1);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(MediaMetricsProviderTest, TestPipelineUMAMediaDrmSoftwareSecure) {
  base::HistogramTester histogram_tester;
  Initialize(false, false, false, kTestOrigin, mojom::MediaURLScheme::kHttps);
  provider_->SetAudioPipelineInfo(
      {false, false, AudioDecoderType::kMojo, EncryptionType::kClear});
  provider_->SetVideoPipelineInfo({false, false, VideoDecoderType::kMediaCodec,
                                   EncryptionType::kEncrypted});
  provider_->SetIsEME();
  provider_->SetHasVideo(VideoCodec::kVP9);
  provider_->SetHasAudio(AudioCodec::kVorbis);
  provider_->SetHasPlayed();
  provider_->SetHaveEnough();
  provider_.reset();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "Media.PipelineStatus.AudioVideo.VP9.MediaDrm.SoftwareSecure",
      PIPELINE_OK, 1);
}

TEST_F(MediaMetricsProviderTest, TestPipelineUMAMediaDrmHardwareSecure) {
  base::HistogramTester histogram_tester;
  Initialize(false, false, false, kTestOrigin, mojom::MediaURLScheme::kHttps);
  provider_->SetAudioPipelineInfo(
      {false, false, AudioDecoderType::kMojo, EncryptionType::kClear});
  provider_->SetVideoPipelineInfo({false, false, VideoDecoderType::kMediaCodec,
                                   EncryptionType::kEncrypted});
  provider_->SetIsEME();
  provider_->SetIsHardwareSecure();
  provider_->SetHasVideo(VideoCodec::kVP9);
  provider_->SetHasAudio(AudioCodec::kVorbis);
  provider_->SetHasPlayed();
  provider_->SetHaveEnough();
  provider_.reset();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "Media.PipelineStatus.AudioVideo.VP9.MediaDrm.HardwareSecure",
      PIPELINE_OK, 1);
}
#endif  // BUILDFLAG(IS_ANDROID)

// Note: Tests for various Acquire* methods are contained with the unittests for
// their respective classes.

#undef EXPECT_UKM
#undef EXPECT_NO_UKM
#undef EXPECT_HAS_UKM

}  // namespace media
