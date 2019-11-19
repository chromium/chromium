// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/watch_time_recorder.h"

#include <stddef.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/hash/hash.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/ukm/test_ukm_recorder.h"
#include "media/base/video_codecs.h"
#include "media/base/watch_time_keys.h"
#include "media/mojo/services/media_metrics_provider.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using UkmEntry = ukm::builders::Media_BasicPlayback;

namespace content {
class RenderFrameHostDelegate;
}  // namespace content

namespace media {

constexpr char kTestOrigin[] = "https://test.google.com/";

class WatchTimeRecorderTest : public testing::Test {
 public:
  WatchTimeRecorderTest()
      : computation_keys_(
            {WatchTimeKey::kAudioSrc, WatchTimeKey::kAudioMse,
             WatchTimeKey::kAudioEme, WatchTimeKey::kAudioVideoSrc,
             WatchTimeKey::kAudioVideoMse, WatchTimeKey::kAudioVideoEme}),
        mtbr_keys_({kMeanTimeBetweenRebuffersAudioSrc,
                    kMeanTimeBetweenRebuffersAudioMse,
                    kMeanTimeBetweenRebuffersAudioEme,
                    kMeanTimeBetweenRebuffersAudioVideoSrc,
                    kMeanTimeBetweenRebuffersAudioVideoMse,
                    kMeanTimeBetweenRebuffersAudioVideoEme}),
        smooth_keys_({kRebuffersCountAudioSrc, kRebuffersCountAudioMse,
                      kRebuffersCountAudioEme, kRebuffersCountAudioVideoSrc,
                      kRebuffersCountAudioVideoMse,
                      kRebuffersCountAudioVideoEme}),
        discard_keys_({kDiscardedWatchTimeAudioSrc, kDiscardedWatchTimeAudioMse,
                       kDiscardedWatchTimeAudioEme,
                       kDiscardedWatchTimeAudioVideoSrc,
                       kDiscardedWatchTimeAudioVideoMse,
                       kDiscardedWatchTimeAudioVideoEme}) {
    source_id_ = test_recorder_->GetNewSourceID();
    ResetMetricRecorders();
    MediaMetricsProvider::Create(
        MediaMetricsProvider::BrowsingMode::kIncognito,
        MediaMetricsProvider::FrameStatus::kTopFrame,
        base::BindRepeating(&WatchTimeRecorderTest::GetSourceId,
                            base::Unretained(this)),
        base::BindRepeating(
            []() { return learning::FeatureValue(0); }) /* origin callback */,
        VideoDecodePerfHistory::SaveCallback(),
        MediaMetricsProvider::GetLearningSessionCallback(),
        base::BindRepeating(
            &WatchTimeRecorderTest::GetRecordAggregateWatchTimeCallback,
            base::Unretained(this)),
        provider_.BindNewPipeAndPassReceiver());
  }

  ~WatchTimeRecorderTest() override { base::RunLoop().RunUntilIdle(); }

  void Initialize(mojom::PlaybackPropertiesPtr properties) {
    provider_->Initialize(properties->is_mse,
                          properties->is_mse ? mojom::MediaURLScheme::kUnknown
                                             : mojom::MediaURLScheme::kHttp);
    provider_->AcquireWatchTimeRecorder(std::move(properties),
                                        wtr_.BindNewPipeAndPassReceiver());
  }

  void Initialize(bool has_audio,
                  bool has_video,
                  bool is_mse,
                  bool is_encrypted) {
    Initialize(mojom::PlaybackProperties::New(
        has_audio, has_video, false, false, is_mse, is_encrypted, false));
  }

  void ExpectWatchTime(const std::vector<base::StringPiece>& keys,
                       base::TimeDelta value) {
    for (int i = 0; i <= static_cast<int>(WatchTimeKey::kWatchTimeKeyMax);
         ++i) {
      const base::StringPiece test_key =
          ConvertWatchTimeKeyToStringForUma(static_cast<WatchTimeKey>(i));
      if (test_key.empty())
        continue;
      auto it = std::find(keys.begin(), keys.end(), test_key);
      if (it == keys.end()) {
        histogram_tester_->ExpectTotalCount(test_key.as_string(), 0);
      } else {
        histogram_tester_->ExpectUniqueSample(test_key.as_string(),
                                              value.InMilliseconds(), 1);
      }
    }
  }

  void ExpectHelper(const std::vector<base::StringPiece>& full_key_list,
                    const std::vector<base::StringPiece>& keys,
                    int64_t value) {
    for (auto key : full_key_list) {
      auto it = std::find(keys.begin(), keys.end(), key);
      if (it == keys.end())
        histogram_tester_->ExpectTotalCount(key.as_string(), 0);
      else
        histogram_tester_->ExpectUniqueSample(key.as_string(), value, 1);
    }
  }

  void ExpectMtbrTime(const std::vector<base::StringPiece>& keys,
                      base::TimeDelta value) {
    ExpectHelper(mtbr_keys_, keys, value.InMilliseconds());
  }

  void ExpectZeroRebuffers(const std::vector<base::StringPiece>& keys) {
    ExpectHelper(smooth_keys_, keys, 0);
  }

  void ExpectRebuffers(const std::vector<base::StringPiece>& keys, int count) {
    ExpectHelper(smooth_keys_, keys, count);
  }

  void ExpectNoUkmWatchTime() {
    // We always add a source in testing.
    ASSERT_EQ(1u, test_recorder_->sources_count());
    ASSERT_EQ(0u, test_recorder_->entries_count());
  }

  void ExpectUkmWatchTime(const std::vector<base::StringPiece>& keys,
                          base::TimeDelta value) {
    const auto& entries =
        test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const auto* entry : entries) {
      test_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestOrigin));
      for (auto key : keys) {
        test_recorder_->ExpectEntryMetric(entry, key.data(),
                                          value.InMilliseconds());
      }
    }
  }

  void ResetMetricRecorders() {
    histogram_tester_.reset(new base::HistogramTester());
    // Ensure cleared global before attempting to create a new TestUkmReporter.
    test_recorder_.reset();
    test_recorder_.reset(new ukm::TestAutoSetUkmRecorder());
    test_recorder_->UpdateSourceURL(source_id_, GURL(kTestOrigin));
  }

  mojom::SecondaryPlaybackPropertiesPtr CreateSecondaryProperties() {
    return mojom::SecondaryPlaybackProperties::New(
        kCodecAAC, kCodecH264, H264PROFILE_MAIN, "", "",
        EncryptionScheme::kUnencrypted, EncryptionScheme::kUnencrypted,
        gfx::Size(800, 600));
  }

  ukm::SourceId GetSourceId() { return source_id_; }

  MediaMetricsProvider::RecordAggregateWatchTimeCallback
  GetRecordAggregateWatchTimeCallback() {
    return base::BindRepeating(
        [](base::WeakPtr<content::RenderFrameHostDelegate> delegate,
           GURL last_committed_url, base::TimeDelta total_watch_time,
           base::TimeDelta time_stamp, bool has_video, bool has_audio) {
          // Do nothing as this mock callback will never be called.
        },
        nullptr, GURL());
  }

  MOCK_METHOD0(GetCurrentMediaTime, base::TimeDelta());

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::Remote<mojom::MediaMetricsProvider> provider_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_recorder_;
  ukm::SourceId source_id_;
  mojo::Remote<mojom::WatchTimeRecorder> wtr_;
  const std::vector<WatchTimeKey> computation_keys_;
  const std::vector<base::StringPiece> mtbr_keys_;
  const std::vector<base::StringPiece> smooth_keys_;
  const std::vector<base::StringPiece> discard_keys_;

  DISALLOW_COPY_AND_ASSIGN(WatchTimeRecorderTest);
};

TEST_F(WatchTimeRecorderTest, TestBasicReporting) {
  constexpr base::TimeDelta kWatchTime1 = base::TimeDelta::FromSeconds(25);
  constexpr base::TimeDelta kWatchTime2 = base::TimeDelta::FromSeconds(50);

  for (int i = 0; i <= static_cast<int>(WatchTimeKey::kWatchTimeKeyMax); ++i) {
    const WatchTimeKey key = static_cast<WatchTimeKey>(i);

    auto key_str = ConvertWatchTimeKeyToStringForUma(key);
    SCOPED_TRACE(key_str.empty() ? base::NumberToString(i)
                                 : key_str.as_string());

    // Values for |is_background| and |is_muted| don't matter in this test since
    // they don't prevent the muted or background keys from being recorded.
    Initialize(true, false, true, true);
    wtr_->UpdateSecondaryProperties(CreateSecondaryProperties());

    wtr_->RecordWatchTime(WatchTimeKey::kWatchTimeKeyMax, kWatchTime1);
    wtr_->RecordWatchTime(key, kWatchTime1);
    wtr_->RecordWatchTime(key, kWatchTime2);
    base::RunLoop().RunUntilIdle();

    // Nothing should be recorded yet since we haven't finalized.
    ExpectWatchTime({}, base::TimeDelta());

    // Only the requested key should be finalized.
    wtr_->FinalizeWatchTime({key});
    base::RunLoop().RunUntilIdle();

    if (!key_str.empty())
      ExpectWatchTime({key_str}, kWatchTime2);

    // These keys are only reported for a full finalize.
    ExpectMtbrTime({}, base::TimeDelta());
    ExpectZeroRebuffers({});
    ExpectNoUkmWatchTime();

    // Verify nothing else is recorded except for what we finalized above.
    ResetMetricRecorders();
    wtr_.reset();
    base::RunLoop().RunUntilIdle();
    ExpectWatchTime({}, base::TimeDelta());
    ExpectMtbrTime({}, base::TimeDelta());
    ExpectZeroRebuffers({});

    switch (key) {
      case WatchTimeKey::kAudioAll:
      case WatchTimeKey::kAudioBackgroundAll:
      case WatchTimeKey::kAudioVideoAll:
      case WatchTimeKey::kAudioVideoBackgroundAll:
      case WatchTimeKey::kAudioVideoMutedAll:
      case WatchTimeKey::kVideoAll:
      case WatchTimeKey::kVideoBackgroundAll:
        ExpectUkmWatchTime({UkmEntry::kWatchTimeName}, kWatchTime2);
        break;

      // These keys are not reported, instead we boolean flags for each type.
      case WatchTimeKey::kAudioMse:
      case WatchTimeKey::kAudioEme:
      case WatchTimeKey::kAudioSrc:
      case WatchTimeKey::kAudioEmbeddedExperience:
      case WatchTimeKey::kAudioBackgroundMse:
      case WatchTimeKey::kAudioBackgroundEme:
      case WatchTimeKey::kAudioBackgroundSrc:
      case WatchTimeKey::kAudioBackgroundEmbeddedExperience:
      case WatchTimeKey::kAudioVideoMse:
      case WatchTimeKey::kAudioVideoEme:
      case WatchTimeKey::kAudioVideoSrc:
      case WatchTimeKey::kAudioVideoEmbeddedExperience:
      case WatchTimeKey::kAudioVideoMutedMse:
      case WatchTimeKey::kAudioVideoMutedEme:
      case WatchTimeKey::kAudioVideoMutedSrc:
      case WatchTimeKey::kAudioVideoMutedEmbeddedExperience:
      case WatchTimeKey::kAudioVideoBackgroundMse:
      case WatchTimeKey::kAudioVideoBackgroundEme:
      case WatchTimeKey::kAudioVideoBackgroundSrc:
      case WatchTimeKey::kAudioVideoBackgroundEmbeddedExperience:
      case WatchTimeKey::kVideoMse:
      case WatchTimeKey::kVideoEme:
      case WatchTimeKey::kVideoSrc:
      case WatchTimeKey::kVideoEmbeddedExperience:
      case WatchTimeKey::kVideoBackgroundMse:
      case WatchTimeKey::kVideoBackgroundEme:
      case WatchTimeKey::kVideoBackgroundSrc:
      case WatchTimeKey::kVideoBackgroundEmbeddedExperience:
        ExpectUkmWatchTime({}, base::TimeDelta());
        break;

      // These keys roll up into the battery watch time field.
      case WatchTimeKey::kAudioBattery:
      case WatchTimeKey::kAudioBackgroundBattery:
      case WatchTimeKey::kAudioVideoBattery:
      case WatchTimeKey::kAudioVideoMutedBattery:
      case WatchTimeKey::kAudioVideoBackgroundBattery:
      case WatchTimeKey::kVideoBattery:
      case WatchTimeKey::kVideoBackgroundBattery:
        ExpectUkmWatchTime({UkmEntry::kWatchTime_BatteryName}, kWatchTime2);
        break;

      // These keys roll up into the AC watch time field.
      case WatchTimeKey::kAudioAc:
      case WatchTimeKey::kAudioBackgroundAc:
      case WatchTimeKey::kAudioVideoAc:
      case WatchTimeKey::kAudioVideoBackgroundAc:
      case WatchTimeKey::kAudioVideoMutedAc:
      case WatchTimeKey::kVideoAc:
      case WatchTimeKey::kVideoBackgroundAc:
        ExpectUkmWatchTime({UkmEntry::kWatchTime_ACName}, kWatchTime2);
        break;

      case WatchTimeKey::kAudioVideoDisplayFullscreen:
      case WatchTimeKey::kAudioVideoMutedDisplayFullscreen:
      case WatchTimeKey::kVideoDisplayFullscreen:
        ExpectUkmWatchTime({UkmEntry::kWatchTime_DisplayFullscreenName},
                           kWatchTime2);
        break;

      case WatchTimeKey::kAudioVideoDisplayInline:
      case WatchTimeKey::kAudioVideoMutedDisplayInline:
      case WatchTimeKey::kVideoDisplayInline:
        ExpectUkmWatchTime({UkmEntry::kWatchTime_DisplayInlineName},
                           kWatchTime2);
        break;

      case WatchTimeKey::kAudioVideoDisplayPictureInPicture:
      case WatchTimeKey::kAudioVideoMutedDisplayPictureInPicture:
      case WatchTimeKey::kVideoDisplayPictureInPicture:
        ExpectUkmWatchTime({UkmEntry::kWatchTime_DisplayPictureInPictureName},
                           kWatchTime2);
        break;

      case WatchTimeKey::kAudioNativeControlsOn:
      case WatchTimeKey::kAudioVideoNativeControlsOn:
      case WatchTimeKey::kAudioVideoMutedNativeControlsOn:
      case WatchTimeKey::kVideoNativeControlsOn:
        ExpectUkmWatchTime({UkmEntry::kWatchTime_NativeControlsOnName},
                           kWatchTime2);
        break;

      case WatchTimeKey::kAudioNativeControlsOff:
      case WatchTimeKey::kAudioVideoNativeControlsOff:
      case WatchTimeKey::kAudioVideoMutedNativeControlsOff:
      case WatchTimeKey::kVideoNativeControlsOff:
        ExpectUkmWatchTime({UkmEntry::kWatchTime_NativeControlsOffName},
                           kWatchTime2);
        break;
    }

    ResetMetricRecorders();
  }
}

TEST_F(WatchTimeRecorderTest, TestRebufferingMetrics) {
  Initialize(true, false, true, true);

  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(50);
  for (auto key : computation_keys_)
    wtr_->RecordWatchTime(key, kWatchTime);
  wtr_->UpdateUnderflowCount(1);
  wtr_->UpdateUnderflowCount(2);

  // Trigger finalization of everything.
  wtr_->FinalizeWatchTime({});
  base::RunLoop().RunUntilIdle();

  ExpectMtbrTime(mtbr_keys_, kWatchTime / 2);
  ExpectRebuffers(smooth_keys_, 2);

  // Now rerun the test without any rebuffering.
  ResetMetricRecorders();
  for (auto key : computation_keys_)
    wtr_->RecordWatchTime(key, kWatchTime);
  wtr_->FinalizeWatchTime({});
  base::RunLoop().RunUntilIdle();

  ExpectMtbrTime({}, base::TimeDelta());
  ExpectZeroRebuffers(smooth_keys_);

  // Now rerun the test with a small amount of watch time and ensure rebuffering
  // isn't recorded because we haven't met the watch time requirements.
  ResetMetricRecorders();
  constexpr base::TimeDelta kWatchTimeShort = base::TimeDelta::FromSeconds(5);
  for (auto key : computation_keys_)
    wtr_->RecordWatchTime(key, kWatchTimeShort);
  wtr_->UpdateUnderflowCount(1);
  wtr_->UpdateUnderflowCount(2);
  wtr_->FinalizeWatchTime({});
  base::RunLoop().RunUntilIdle();

  // Nothing should be logged since this doesn't meet requirements.
  ExpectMtbrTime({}, base::TimeDelta());
  for (auto key : smooth_keys_)
    histogram_tester_->ExpectTotalCount(key.as_string(), 0);
}

TEST_F(WatchTimeRecorderTest, TestDiscardMetrics) {
  Initialize(true, false, true, true);
  wtr_->UpdateSecondaryProperties(CreateSecondaryProperties());

  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(5);
  for (auto key : computation_keys_)
    wtr_->RecordWatchTime(key, kWatchTime);

  // Trigger finalization of everything.
  wtr_.reset();
  base::RunLoop().RunUntilIdle();

  // No standard watch time should be recorded because it falls below the
  // reporting threshold.
  ExpectWatchTime({}, base::TimeDelta());

  // Verify the time was instead logged to the discard keys.
  for (auto key : discard_keys_) {
    histogram_tester_->ExpectUniqueSample(key.as_string(),
                                          kWatchTime.InMilliseconds(), 1);
  }

  // UKM watch time won't be logged because we aren't sending "All" keys.
  ExpectUkmWatchTime({}, base::TimeDelta());
}

#define EXPECT_UKM(name, value) \
  test_recorder_->ExpectEntryMetric(entry, name, value)
#define EXPECT_NO_UKM(name) \
  EXPECT_FALSE(test_recorder_->EntryHasMetric(entry, name))
#define EXPECT_HAS_UKM(name) \
  EXPECT_TRUE(test_recorder_->EntryHasMetric(entry, name));

TEST_F(WatchTimeRecorderTest, TestFinalizeNoDuplication) {
  mojom::PlaybackPropertiesPtr properties = mojom::PlaybackProperties::New(
      true, true, false, false, false, false, false);
  mojom::SecondaryPlaybackPropertiesPtr secondary_properties =
      CreateSecondaryProperties();
  Initialize(properties.Clone());
  wtr_->UpdateSecondaryProperties(secondary_properties.Clone());

  // Verify that UKM is reported along with the watch time.
  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(4);
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoAll, kWatchTime);

  // Finalize everything. UKM is only recorded at destruction, so this should do
  // nothing.
  wtr_->FinalizeWatchTime({});
  base::RunLoop().RunUntilIdle();

  // No watch time should have been recorded since this is below the UMA report
  // threshold.
  ExpectWatchTime({}, base::TimeDelta());
  ExpectMtbrTime({}, base::TimeDelta());
  ExpectZeroRebuffers({});
  ExpectNoUkmWatchTime();

  const auto& empty_entries =
      test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(0u, empty_entries.size());

  // Verify UKM is logged at destruction time.
  ResetMetricRecorders();
  wtr_.reset();
  base::RunLoop().RunUntilIdle();
  const auto& entries = test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    test_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestOrigin));

    EXPECT_UKM(UkmEntry::kIsBackgroundName, properties->is_background);
    EXPECT_UKM(UkmEntry::kIsMutedName, properties->is_muted);
    EXPECT_UKM(UkmEntry::kAudioCodecName, secondary_properties->audio_codec);
    EXPECT_UKM(UkmEntry::kVideoCodecName, secondary_properties->video_codec);
    EXPECT_UKM(UkmEntry::kVideoCodecProfileName,
               secondary_properties->video_codec_profile);
    EXPECT_UKM(UkmEntry::kHasAudioName, properties->has_audio);
    EXPECT_UKM(UkmEntry::kHasVideoName, properties->has_video);
    EXPECT_UKM(
        UkmEntry::kAudioEncryptionSchemeName,
        static_cast<int64_t>(secondary_properties->audio_encryption_scheme));
    EXPECT_UKM(
        UkmEntry::kVideoEncryptionSchemeName,
        static_cast<int64_t>(secondary_properties->video_encryption_scheme));
    EXPECT_UKM(UkmEntry::kIsEMEName, properties->is_eme);
    EXPECT_UKM(UkmEntry::kIsMSEName, properties->is_mse);
    EXPECT_UKM(UkmEntry::kLastPipelineStatusName, PIPELINE_OK);
    EXPECT_UKM(UkmEntry::kRebuffersCountName, 0);
    EXPECT_UKM(UkmEntry::kCompletedRebuffersCountName, 0);
    EXPECT_UKM(UkmEntry::kCompletedRebuffersDurationName, 0);
    EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
               secondary_properties->natural_size.width());
    EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
               secondary_properties->natural_size.height());
    EXPECT_UKM(UkmEntry::kWatchTimeName, kWatchTime.InMilliseconds());
    EXPECT_UKM(UkmEntry::kAudioDecoderNameName, 0);
    EXPECT_UKM(UkmEntry::kVideoDecoderNameName, 0);
    EXPECT_UKM(UkmEntry::kAutoplayInitiatedName, false);
    EXPECT_HAS_UKM(UkmEntry::kPlayerIDName);

    EXPECT_NO_UKM(UkmEntry::kMeanTimeBetweenRebuffersName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_ACName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_BatteryName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_NativeControlsOnName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_NativeControlsOffName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayFullscreenName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayInlineName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayPictureInPictureName);
  }
}

TEST_F(WatchTimeRecorderTest, FinalizeWithoutWatchTime) {
  mojom::PlaybackPropertiesPtr properties = mojom::PlaybackProperties::New(
      true, true, false, false, false, false, false);
  mojom::SecondaryPlaybackPropertiesPtr secondary_properties =
      CreateSecondaryProperties();
  Initialize(properties.Clone());
  wtr_->UpdateSecondaryProperties(secondary_properties.Clone());

  // Finalize everything. UKM is only recorded at destruction, so this should do
  // nothing.
  wtr_->FinalizeWatchTime({});
  base::RunLoop().RunUntilIdle();

  // No watch time should have been recorded even though a finalize event will
  // be sent, however a UKM entry with the playback properties will still be
  // generated.
  ExpectWatchTime({}, base::TimeDelta());
  ExpectMtbrTime({}, base::TimeDelta());
  ExpectZeroRebuffers({});
  ExpectNoUkmWatchTime();

  const auto& empty_entries =
      test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(0u, empty_entries.size());

  // Destructing the recorder should generate a UKM report though.
  ResetMetricRecorders();
  wtr_.reset();
  base::RunLoop().RunUntilIdle();
  const auto& entries = test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    test_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestOrigin));

    EXPECT_UKM(UkmEntry::kIsBackgroundName, properties->is_background);
    EXPECT_UKM(UkmEntry::kIsMutedName, properties->is_muted);
    EXPECT_UKM(UkmEntry::kAudioCodecName, secondary_properties->audio_codec);
    EXPECT_UKM(UkmEntry::kVideoCodecName, secondary_properties->video_codec);
    EXPECT_UKM(UkmEntry::kVideoCodecProfileName,
               secondary_properties->video_codec_profile);
    EXPECT_UKM(UkmEntry::kHasAudioName, properties->has_audio);
    EXPECT_UKM(UkmEntry::kHasVideoName, properties->has_video);
    EXPECT_UKM(
        UkmEntry::kAudioEncryptionSchemeName,
        static_cast<int64_t>(secondary_properties->audio_encryption_scheme));
    EXPECT_UKM(
        UkmEntry::kVideoEncryptionSchemeName,
        static_cast<int64_t>(secondary_properties->video_encryption_scheme));
    EXPECT_UKM(UkmEntry::kIsEMEName, properties->is_eme);
    EXPECT_UKM(UkmEntry::kIsMSEName, properties->is_mse);
    EXPECT_UKM(UkmEntry::kLastPipelineStatusName, PIPELINE_OK);
    EXPECT_UKM(UkmEntry::kRebuffersCountName, 0);
    EXPECT_UKM(UkmEntry::kCompletedRebuffersCountName, 0);
    EXPECT_UKM(UkmEntry::kCompletedRebuffersDurationName, 0);
    EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
               secondary_properties->natural_size.width());
    EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
               secondary_properties->natural_size.height());
    EXPECT_UKM(UkmEntry::kAudioDecoderNameName, 0);
    EXPECT_UKM(UkmEntry::kVideoDecoderNameName, 0);
    EXPECT_UKM(UkmEntry::kAutoplayInitiatedName, false);
    EXPECT_HAS_UKM(UkmEntry::kPlayerIDName);

    EXPECT_NO_UKM(UkmEntry::kMeanTimeBetweenRebuffersName);
    EXPECT_NO_UKM(UkmEntry::kWatchTimeName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_ACName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_BatteryName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_NativeControlsOnName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_NativeControlsOffName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayFullscreenName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayInlineName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayPictureInPictureName);
  }
}

TEST_F(WatchTimeRecorderTest, BasicUkmAudioVideo) {
  mojom::PlaybackPropertiesPtr properties = mojom::PlaybackProperties::New(
      true, true, false, false, false, false, false);
  mojom::SecondaryPlaybackPropertiesPtr secondary_properties =
      mojom::SecondaryPlaybackProperties::New(
          kCodecAAC, kCodecH264, H264PROFILE_MAIN, "", "",
          EncryptionScheme::kCenc, EncryptionScheme::kCbcs,
          gfx::Size(800, 600));
  Initialize(properties.Clone());
  wtr_->UpdateSecondaryProperties(secondary_properties.Clone());

  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(4);
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoAll, kWatchTime);
  wtr_.reset();
  base::RunLoop().RunUntilIdle();

  const auto& entries = test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    test_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestOrigin));

    EXPECT_UKM(UkmEntry::kWatchTimeName, kWatchTime.InMilliseconds());
    EXPECT_UKM(UkmEntry::kIsBackgroundName, properties->is_background);
    EXPECT_UKM(UkmEntry::kIsMutedName, properties->is_muted);
    EXPECT_UKM(UkmEntry::kAudioCodecName, secondary_properties->audio_codec);
    EXPECT_UKM(UkmEntry::kVideoCodecName, secondary_properties->video_codec);
    EXPECT_UKM(UkmEntry::kVideoCodecProfileName,
               secondary_properties->video_codec_profile);
    EXPECT_UKM(UkmEntry::kHasAudioName, properties->has_audio);
    EXPECT_UKM(UkmEntry::kHasVideoName, properties->has_video);
    EXPECT_UKM(
        UkmEntry::kAudioEncryptionSchemeName,
        static_cast<int64_t>(secondary_properties->audio_encryption_scheme));
    EXPECT_UKM(
        UkmEntry::kVideoEncryptionSchemeName,
        static_cast<int64_t>(secondary_properties->video_encryption_scheme));
    EXPECT_UKM(UkmEntry::kIsEMEName, properties->is_eme);
    EXPECT_UKM(UkmEntry::kIsMSEName, properties->is_mse);
    EXPECT_UKM(UkmEntry::kLastPipelineStatusName, PIPELINE_OK);
    EXPECT_UKM(UkmEntry::kRebuffersCountName, 0);
    EXPECT_UKM(UkmEntry::kCompletedRebuffersCountName, 0);
    EXPECT_UKM(UkmEntry::kCompletedRebuffersDurationName, 0);
    EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
               secondary_properties->natural_size.width());
    EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
               secondary_properties->natural_size.height());
    EXPECT_HAS_UKM(UkmEntry::kPlayerIDName);
    EXPECT_UKM(UkmEntry::kAudioDecoderNameName, 0);
    EXPECT_UKM(UkmEntry::kVideoDecoderNameName, 0);
    EXPECT_UKM(UkmEntry::kAutoplayInitiatedName, false);

    EXPECT_NO_UKM(UkmEntry::kMeanTimeBetweenRebuffersName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_ACName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_BatteryName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_NativeControlsOnName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_NativeControlsOffName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayFullscreenName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayInlineName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayPictureInPictureName);
  }
}

TEST_F(WatchTimeRecorderTest, BasicUkmAudioVideoWithExtras) {
  mojom::PlaybackPropertiesPtr properties = mojom::PlaybackProperties::New(
      true, true, false, false, true, true, false);
  mojom::SecondaryPlaybackPropertiesPtr secondary_properties =
      mojom::SecondaryPlaybackProperties::New(
          kCodecOpus, kCodecVP9, VP9PROFILE_PROFILE0, "", "",
          EncryptionScheme::kUnencrypted, EncryptionScheme::kUnencrypted,
          gfx::Size(800, 600));
  Initialize(properties.Clone());
  wtr_->UpdateSecondaryProperties(secondary_properties.Clone());

  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(54);
  const base::TimeDelta kWatchTime2 = kWatchTime * 2;
  const base::TimeDelta kWatchTime3 = kWatchTime / 3;
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoAll, kWatchTime2);
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoAc, kWatchTime);

  // Ensure partial finalize does not affect final report.
  wtr_->FinalizeWatchTime({WatchTimeKey::kAudioVideoAc});
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoBattery, kWatchTime);
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoNativeControlsOn, kWatchTime);
  wtr_->FinalizeWatchTime({WatchTimeKey::kAudioVideoNativeControlsOn});
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoNativeControlsOff, kWatchTime);
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoDisplayFullscreen,
                        kWatchTime3);
  wtr_->FinalizeWatchTime({WatchTimeKey::kAudioVideoDisplayFullscreen});
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoDisplayInline, kWatchTime3);
  wtr_->FinalizeWatchTime({WatchTimeKey::kAudioVideoDisplayInline});
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoDisplayPictureInPicture,
                        kWatchTime3);
  wtr_->UpdateUnderflowCount(3);
  constexpr base::TimeDelta kUnderflowDuration =
      base::TimeDelta::FromMilliseconds(500);
  wtr_->UpdateUnderflowDuration(2, kUnderflowDuration);
  wtr_->UpdateVideoDecodeStats(10, 2);
  wtr_->OnError(PIPELINE_ERROR_DECODE);

  secondary_properties->audio_decoder_name = "MojoAudioDecoder";
  secondary_properties->video_decoder_name = "MojoVideoDecoder";
  wtr_->UpdateSecondaryProperties(secondary_properties.Clone());

  wtr_->SetAutoplayInitiated(true);

  wtr_->OnDurationChanged(base::TimeDelta::FromSeconds(9500));

  wtr_.reset();
  base::RunLoop().RunUntilIdle();

  const auto& entries = test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    test_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestOrigin));
    EXPECT_UKM(UkmEntry::kWatchTimeName, kWatchTime2.InMilliseconds());
    EXPECT_UKM(UkmEntry::kWatchTime_ACName, kWatchTime.InMilliseconds());
    EXPECT_UKM(UkmEntry::kWatchTime_BatteryName, kWatchTime.InMilliseconds());
    EXPECT_UKM(UkmEntry::kWatchTime_NativeControlsOnName,
               kWatchTime.InMilliseconds());
    EXPECT_UKM(UkmEntry::kWatchTime_NativeControlsOffName,
               kWatchTime.InMilliseconds());
    EXPECT_UKM(UkmEntry::kWatchTime_DisplayFullscreenName,
               kWatchTime3.InMilliseconds());
    EXPECT_UKM(UkmEntry::kWatchTime_DisplayInlineName,
               kWatchTime3.InMilliseconds());
    EXPECT_UKM(UkmEntry::kWatchTime_DisplayPictureInPictureName,
               kWatchTime3.InMilliseconds());
    EXPECT_UKM(UkmEntry::kMeanTimeBetweenRebuffersName,
               kWatchTime2.InMilliseconds() / 3);
    EXPECT_HAS_UKM(UkmEntry::kPlayerIDName);

    // Values taken from .cc private enumeration (and should never change).
    EXPECT_UKM(UkmEntry::kAudioDecoderNameName, 2);
    EXPECT_UKM(UkmEntry::kVideoDecoderNameName, 5);

    // Duration should be rounded up.
    EXPECT_UKM(UkmEntry::kDurationName, 10000000);

    EXPECT_UKM(UkmEntry::kIsBackgroundName, properties->is_background);
    EXPECT_UKM(UkmEntry::kIsMutedName, properties->is_muted);
    EXPECT_UKM(UkmEntry::kAudioCodecName, secondary_properties->audio_codec);
    EXPECT_UKM(UkmEntry::kVideoCodecName, secondary_properties->video_codec);
    EXPECT_UKM(UkmEntry::kVideoCodecProfileName,
               secondary_properties->video_codec_profile);
    EXPECT_UKM(UkmEntry::kHasAudioName, properties->has_audio);
    EXPECT_UKM(UkmEntry::kHasVideoName, properties->has_video);
    EXPECT_UKM(
        UkmEntry::kAudioEncryptionSchemeName,
        static_cast<int64_t>(secondary_properties->audio_encryption_scheme));
    EXPECT_UKM(
        UkmEntry::kVideoEncryptionSchemeName,
        static_cast<int64_t>(secondary_properties->video_encryption_scheme));
    EXPECT_UKM(UkmEntry::kIsEMEName, properties->is_eme);
    EXPECT_UKM(UkmEntry::kIsMSEName, properties->is_mse);
    EXPECT_UKM(UkmEntry::kLastPipelineStatusName, PIPELINE_ERROR_DECODE);
    EXPECT_UKM(UkmEntry::kRebuffersCountName, 3);
    EXPECT_UKM(UkmEntry::kCompletedRebuffersCountName, 2);
    EXPECT_UKM(UkmEntry::kCompletedRebuffersDurationName,
               kUnderflowDuration.InMilliseconds());
    EXPECT_UKM(UkmEntry::kVideoFramesDecodedName, 10);
    EXPECT_UKM(UkmEntry::kVideoFramesDroppedName, 2);
    EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
               secondary_properties->natural_size.width());
    EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
               secondary_properties->natural_size.height());
    EXPECT_UKM(UkmEntry::kAutoplayInitiatedName, true);
  }
}

TEST_F(WatchTimeRecorderTest, BasicUkmAudioVideoBackgroundMuted) {
  mojom::PlaybackPropertiesPtr properties = mojom::PlaybackProperties::New(
      true, true, true, true, false, false, false);
  mojom::SecondaryPlaybackPropertiesPtr secondary_properties =
      CreateSecondaryProperties();
  Initialize(properties.Clone());
  wtr_->UpdateSecondaryProperties(secondary_properties.Clone());

  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(54);
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoBackgroundAll, kWatchTime);
  wtr_.reset();
  base::RunLoop().RunUntilIdle();

  const auto& entries = test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    test_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestOrigin));

    EXPECT_UKM(UkmEntry::kWatchTimeName, kWatchTime.InMilliseconds());
    EXPECT_UKM(UkmEntry::kIsBackgroundName, properties->is_background);
    EXPECT_UKM(UkmEntry::kIsMutedName, properties->is_muted);
    EXPECT_UKM(UkmEntry::kAudioCodecName, secondary_properties->audio_codec);
    EXPECT_UKM(UkmEntry::kVideoCodecName, secondary_properties->video_codec);
    EXPECT_UKM(UkmEntry::kVideoCodecProfileName,
               secondary_properties->video_codec_profile);
    EXPECT_UKM(UkmEntry::kHasAudioName, properties->has_audio);
    EXPECT_UKM(UkmEntry::kHasVideoName, properties->has_video);
    EXPECT_UKM(
        UkmEntry::kAudioEncryptionSchemeName,
        static_cast<int64_t>(secondary_properties->audio_encryption_scheme));
    EXPECT_UKM(
        UkmEntry::kVideoEncryptionSchemeName,
        static_cast<int64_t>(secondary_properties->video_encryption_scheme));
    EXPECT_UKM(UkmEntry::kIsEMEName, properties->is_eme);
    EXPECT_UKM(UkmEntry::kIsMSEName, properties->is_mse);
    EXPECT_UKM(UkmEntry::kLastPipelineStatusName, PIPELINE_OK);
    EXPECT_UKM(UkmEntry::kRebuffersCountName, 0);
    EXPECT_UKM(UkmEntry::kCompletedRebuffersCountName, 0);
    EXPECT_UKM(UkmEntry::kCompletedRebuffersDurationName, 0);
    EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
               secondary_properties->natural_size.width());
    EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
               secondary_properties->natural_size.height());
    EXPECT_HAS_UKM(UkmEntry::kPlayerIDName);
    EXPECT_UKM(UkmEntry::kAudioDecoderNameName, 0);
    EXPECT_UKM(UkmEntry::kVideoDecoderNameName, 0);
    EXPECT_UKM(UkmEntry::kAutoplayInitiatedName, false);

    EXPECT_NO_UKM(UkmEntry::kDurationName);
    EXPECT_NO_UKM(UkmEntry::kMeanTimeBetweenRebuffersName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_ACName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_BatteryName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_NativeControlsOnName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_NativeControlsOffName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayFullscreenName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayInlineName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayPictureInPictureName);
  }
}

TEST_F(WatchTimeRecorderTest, BasicUkmAudioVideoDuration) {
  mojom::PlaybackPropertiesPtr properties = mojom::PlaybackProperties::New(
      true, true, false, false, false, false, false);
  mojom::SecondaryPlaybackPropertiesPtr secondary_properties =
      CreateSecondaryProperties();
  Initialize(properties.Clone());
  wtr_->UpdateSecondaryProperties(secondary_properties.Clone());

  wtr_->OnDurationChanged(base::TimeDelta::FromSeconds(12345));
  wtr_.reset();
  base::RunLoop().RunUntilIdle();

  const auto& entries = test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    test_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestOrigin));

    EXPECT_UKM(UkmEntry::kIsBackgroundName, properties->is_background);
    EXPECT_UKM(UkmEntry::kIsMutedName, properties->is_muted);
    EXPECT_UKM(UkmEntry::kAudioCodecName, secondary_properties->audio_codec);
    EXPECT_UKM(UkmEntry::kVideoCodecName, secondary_properties->video_codec);
    EXPECT_UKM(UkmEntry::kVideoCodecProfileName,
               secondary_properties->video_codec_profile);
    EXPECT_UKM(UkmEntry::kHasAudioName, properties->has_audio);
    EXPECT_UKM(UkmEntry::kHasVideoName, properties->has_video);
    EXPECT_UKM(
        UkmEntry::kAudioEncryptionSchemeName,
        static_cast<int64_t>(secondary_properties->audio_encryption_scheme));
    EXPECT_UKM(
        UkmEntry::kVideoEncryptionSchemeName,
        static_cast<int64_t>(secondary_properties->video_encryption_scheme));
    EXPECT_UKM(UkmEntry::kIsEMEName, properties->is_eme);
    EXPECT_UKM(UkmEntry::kIsMSEName, properties->is_mse);
    EXPECT_UKM(UkmEntry::kLastPipelineStatusName, PIPELINE_OK);
    EXPECT_UKM(UkmEntry::kRebuffersCountName, 0);
    EXPECT_UKM(UkmEntry::kCompletedRebuffersCountName, 0);
    EXPECT_UKM(UkmEntry::kCompletedRebuffersDurationName, 0);
    EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
               secondary_properties->natural_size.width());
    EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
               secondary_properties->natural_size.height());
    EXPECT_HAS_UKM(UkmEntry::kPlayerIDName);
    EXPECT_UKM(UkmEntry::kAudioDecoderNameName, 0);
    EXPECT_UKM(UkmEntry::kVideoDecoderNameName, 0);
    EXPECT_UKM(UkmEntry::kAutoplayInitiatedName, false);

    // Duration should be rounded to the most significant digit.
    EXPECT_UKM(UkmEntry::kDurationName, 10000000);

    EXPECT_NO_UKM(UkmEntry::kMeanTimeBetweenRebuffersName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_ACName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_BatteryName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_NativeControlsOnName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_NativeControlsOffName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayFullscreenName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayInlineName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayPictureInPictureName);
  }
}

TEST_F(WatchTimeRecorderTest, BasicUkmAudioVideoDurationInfinite) {
  mojom::PlaybackPropertiesPtr properties = mojom::PlaybackProperties::New(
      true, true, false, false, false, false, false);
  mojom::SecondaryPlaybackPropertiesPtr secondary_properties =
      CreateSecondaryProperties();
  Initialize(properties.Clone());
  wtr_->UpdateSecondaryProperties(secondary_properties.Clone());

  wtr_->OnDurationChanged(kInfiniteDuration);
  wtr_.reset();
  base::RunLoop().RunUntilIdle();

  const auto& entries = test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    test_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestOrigin));

    EXPECT_UKM(UkmEntry::kIsBackgroundName, properties->is_background);
    EXPECT_UKM(UkmEntry::kIsMutedName, properties->is_muted);
    EXPECT_UKM(UkmEntry::kAudioCodecName, secondary_properties->audio_codec);
    EXPECT_UKM(UkmEntry::kVideoCodecName, secondary_properties->video_codec);
    EXPECT_UKM(UkmEntry::kVideoCodecProfileName,
               secondary_properties->video_codec_profile);
    EXPECT_UKM(UkmEntry::kHasAudioName, properties->has_audio);
    EXPECT_UKM(UkmEntry::kHasVideoName, properties->has_video);
    EXPECT_UKM(
        UkmEntry::kAudioEncryptionSchemeName,
        static_cast<int64_t>(secondary_properties->audio_encryption_scheme));
    EXPECT_UKM(
        UkmEntry::kVideoEncryptionSchemeName,
        static_cast<int64_t>(secondary_properties->video_encryption_scheme));
    EXPECT_UKM(UkmEntry::kIsEMEName, properties->is_eme);
    EXPECT_UKM(UkmEntry::kIsMSEName, properties->is_mse);
    EXPECT_UKM(UkmEntry::kLastPipelineStatusName, PIPELINE_OK);
    EXPECT_UKM(UkmEntry::kRebuffersCountName, 0);
    EXPECT_UKM(UkmEntry::kCompletedRebuffersCountName, 0);
    EXPECT_UKM(UkmEntry::kCompletedRebuffersDurationName, 0);
    EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
               secondary_properties->natural_size.width());
    EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
               secondary_properties->natural_size.height());
    EXPECT_HAS_UKM(UkmEntry::kPlayerIDName);
    EXPECT_UKM(UkmEntry::kAudioDecoderNameName, 0);
    EXPECT_UKM(UkmEntry::kVideoDecoderNameName, 0);
    EXPECT_UKM(UkmEntry::kAutoplayInitiatedName, false);

    // Duration should be unrecorded when infinite.
    EXPECT_NO_UKM(UkmEntry::kDurationName);
    EXPECT_NO_UKM(UkmEntry::kWatchTimeName);
    EXPECT_NO_UKM(UkmEntry::kMeanTimeBetweenRebuffersName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_ACName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_BatteryName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_NativeControlsOnName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_NativeControlsOffName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayFullscreenName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayInlineName);
    EXPECT_NO_UKM(UkmEntry::kWatchTime_DisplayPictureInPictureName);
  }
}

// Might happen due to timing issues, so ensure no crashes.
TEST_F(WatchTimeRecorderTest, NoSecondaryProperties) {
  mojom::PlaybackPropertiesPtr properties = mojom::PlaybackProperties::New(
      true, true, false, false, true, true, false);
  Initialize(properties.Clone());

  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(54);
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoAll, kWatchTime);
  wtr_.reset();
  base::RunLoop().RunUntilIdle();
  const auto& entries = test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

TEST_F(WatchTimeRecorderTest, SingleSecondaryPropertiesUnknownToKnown) {
  mojom::PlaybackPropertiesPtr properties = mojom::PlaybackProperties::New(
      true, true, false, false, true, true, false);
  mojom::SecondaryPlaybackPropertiesPtr secondary_properties1 =
      mojom::SecondaryPlaybackProperties::New(
          kUnknownAudioCodec, kUnknownVideoCodec, VIDEO_CODEC_PROFILE_UNKNOWN,
          "", "", EncryptionScheme::kUnencrypted,
          EncryptionScheme::kUnencrypted, gfx::Size(800, 600));
  Initialize(properties.Clone());
  wtr_->UpdateSecondaryProperties(secondary_properties1.Clone());

  constexpr base::TimeDelta kWatchTime = base::TimeDelta::FromSeconds(54);
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoAll, kWatchTime);

  mojom::SecondaryPlaybackPropertiesPtr secondary_properties2 =
      mojom::SecondaryPlaybackProperties::New(
          kCodecAAC, kCodecH264, H264PROFILE_MAIN, "FFmpegAudioDecoder",
          "FFmpegVideoDecoder", EncryptionScheme::kUnencrypted,
          EncryptionScheme::kUnencrypted, gfx::Size(800, 600));
  wtr_->UpdateSecondaryProperties(secondary_properties2.Clone());

  wtr_.reset();
  base::RunLoop().RunUntilIdle();

  // Since we only transitioned unknown values to known values, there should be
  // only a single UKM entry.
  const auto& entries = test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    test_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestOrigin));
    EXPECT_UKM(UkmEntry::kIsBackgroundName, properties->is_background);
    EXPECT_UKM(UkmEntry::kIsMutedName, properties->is_muted);
    EXPECT_UKM(UkmEntry::kHasAudioName, properties->has_audio);
    EXPECT_UKM(UkmEntry::kHasVideoName, properties->has_video);
    EXPECT_UKM(UkmEntry::kIsEMEName, properties->is_eme);
    EXPECT_UKM(UkmEntry::kIsMSEName, properties->is_mse);
    EXPECT_UKM(UkmEntry::kAutoplayInitiatedName, false);
    EXPECT_UKM(UkmEntry::kLastPipelineStatusName, PIPELINE_OK);
    EXPECT_HAS_UKM(UkmEntry::kPlayerIDName);
    EXPECT_UKM(UkmEntry::kWatchTimeName, kWatchTime.InMilliseconds());
    EXPECT_UKM(UkmEntry::kAudioDecoderNameName, 1);
    EXPECT_UKM(UkmEntry::kVideoDecoderNameName, 2);
    EXPECT_UKM(UkmEntry::kRebuffersCountName, 0);
    EXPECT_UKM(UkmEntry::kCompletedRebuffersCountName, 0);
    EXPECT_UKM(UkmEntry::kCompletedRebuffersDurationName, 0);
    EXPECT_UKM(UkmEntry::kAudioCodecName, secondary_properties2->audio_codec);
    EXPECT_UKM(UkmEntry::kVideoCodecName, secondary_properties2->video_codec);
    EXPECT_UKM(UkmEntry::kVideoCodecProfileName,
               secondary_properties2->video_codec_profile);
    EXPECT_UKM(
        UkmEntry::kAudioEncryptionSchemeName,
        static_cast<int64_t>(secondary_properties2->audio_encryption_scheme));
    EXPECT_UKM(
        UkmEntry::kVideoEncryptionSchemeName,
        static_cast<int64_t>(secondary_properties2->video_encryption_scheme));
    EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
               secondary_properties2->natural_size.width());
    EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
               secondary_properties2->natural_size.height());
    EXPECT_NO_UKM(UkmEntry::kDurationName);
  }
}

TEST_F(WatchTimeRecorderTest, MultipleSecondaryPropertiesNoFinalize) {
  mojom::PlaybackPropertiesPtr properties = mojom::PlaybackProperties::New(
      true, true, false, false, true, true, false);
  mojom::SecondaryPlaybackPropertiesPtr secondary_properties1 =
      mojom::SecondaryPlaybackProperties::New(
          kCodecOpus, kCodecVP9, VP9PROFILE_PROFILE0, "MojoAudioDecoder",
          "MojoVideoDecoder", EncryptionScheme::kUnencrypted,
          EncryptionScheme::kUnencrypted, gfx::Size(400, 300));
  Initialize(properties.Clone());
  wtr_->UpdateSecondaryProperties(secondary_properties1.Clone());

  constexpr base::TimeDelta kUnderflowDuration =
      base::TimeDelta::FromMilliseconds(250);
  constexpr base::TimeDelta kWatchTime1 = base::TimeDelta::FromSeconds(54);
  const int kUnderflowCount1 = 2;
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoAll, kWatchTime1);
  wtr_->UpdateUnderflowCount(kUnderflowCount1);
  wtr_->UpdateUnderflowDuration(kUnderflowCount1, kUnderflowDuration);

  constexpr int kDecodedFrameCount1 = 10;
  constexpr int kDroppedFrameCount1 = 2;
  wtr_->UpdateVideoDecodeStats(kDecodedFrameCount1, kDroppedFrameCount1);

  mojom::SecondaryPlaybackPropertiesPtr secondary_properties2 =
      mojom::SecondaryPlaybackProperties::New(
          kCodecAAC, kCodecH264, H264PROFILE_MAIN, "FFmpegAudioDecoder",
          "FFmpegVideoDecoder", EncryptionScheme::kCenc,
          EncryptionScheme::kCenc, gfx::Size(800, 600));
  wtr_->UpdateSecondaryProperties(secondary_properties2.Clone());

  constexpr base::TimeDelta kWatchTime2 = base::TimeDelta::FromSeconds(25);
  const int kUnderflowCount2 = 3;

  // Watch time and underflow counts continue to accumulate during property
  // changes, so we report the sum here instead of just kWatchTime2.
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoAll,
                        kWatchTime1 + kWatchTime2);
  wtr_->UpdateUnderflowCount(kUnderflowCount1 + kUnderflowCount2);
  wtr_->OnError(PIPELINE_ERROR_DECODE);
  wtr_->OnDurationChanged(base::TimeDelta::FromSeconds(5125));

  constexpr int kDecodedFrameCount2 = 20;
  constexpr int kDroppedFrameCount2 = 10;
  wtr_->UpdateVideoDecodeStats(kDecodedFrameCount1 + kDecodedFrameCount2,
                               kDroppedFrameCount1 + kDroppedFrameCount2);

  wtr_.reset();
  base::RunLoop().RunUntilIdle();

  // All records should have the following:
  const auto& entries = test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(2u, entries.size());
  for (const auto* entry : entries) {
    test_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestOrigin));
    EXPECT_UKM(UkmEntry::kIsBackgroundName, properties->is_background);
    EXPECT_UKM(UkmEntry::kIsMutedName, properties->is_muted);
    EXPECT_UKM(UkmEntry::kHasAudioName, properties->has_audio);
    EXPECT_UKM(UkmEntry::kHasVideoName, properties->has_video);
    EXPECT_UKM(UkmEntry::kIsEMEName, properties->is_eme);
    EXPECT_UKM(UkmEntry::kIsMSEName, properties->is_mse);
    EXPECT_UKM(UkmEntry::kAutoplayInitiatedName, false);
    EXPECT_UKM(UkmEntry::kDurationName, 5000000);
    EXPECT_HAS_UKM(UkmEntry::kPlayerIDName);

    // All records inherit the final pipeline status code.
    EXPECT_UKM(UkmEntry::kLastPipelineStatusName, PIPELINE_ERROR_DECODE);
  }

  // The first record should have...
  auto* entry = entries[0];
  EXPECT_UKM(UkmEntry::kWatchTimeName, kWatchTime1.InMilliseconds());
  EXPECT_UKM(UkmEntry::kMeanTimeBetweenRebuffersName,
             kWatchTime1.InMilliseconds() / kUnderflowCount1);
  EXPECT_UKM(UkmEntry::kAudioDecoderNameName, 2);
  EXPECT_UKM(UkmEntry::kVideoDecoderNameName, 5);
  EXPECT_UKM(UkmEntry::kRebuffersCountName, kUnderflowCount1);
  EXPECT_UKM(UkmEntry::kCompletedRebuffersCountName, kUnderflowCount1);
  EXPECT_UKM(UkmEntry::kCompletedRebuffersDurationName,
             kUnderflowDuration.InMilliseconds());
  EXPECT_UKM(UkmEntry::kVideoFramesDecodedName, kDecodedFrameCount1);
  EXPECT_UKM(UkmEntry::kVideoFramesDroppedName, kDroppedFrameCount1);
  EXPECT_UKM(UkmEntry::kAudioCodecName, secondary_properties1->audio_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecName, secondary_properties1->video_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecProfileName,
             secondary_properties1->video_codec_profile);
  EXPECT_UKM(
      UkmEntry::kAudioEncryptionSchemeName,
      static_cast<int64_t>(secondary_properties1->audio_encryption_scheme));
  EXPECT_UKM(
      UkmEntry::kVideoEncryptionSchemeName,
      static_cast<int64_t>(secondary_properties1->video_encryption_scheme));
  EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
             secondary_properties1->natural_size.width());
  EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
             secondary_properties1->natural_size.height());

  // The second record should have...
  entry = entries[1];
  EXPECT_UKM(UkmEntry::kWatchTimeName, kWatchTime2.InMilliseconds());
  EXPECT_UKM(UkmEntry::kMeanTimeBetweenRebuffersName,
             kWatchTime2.InMilliseconds() / kUnderflowCount2);
  EXPECT_UKM(UkmEntry::kAudioDecoderNameName, 1);
  EXPECT_UKM(UkmEntry::kVideoDecoderNameName, 2);
  EXPECT_UKM(UkmEntry::kRebuffersCountName, kUnderflowCount2);
  EXPECT_UKM(UkmEntry::kCompletedRebuffersCountName, 0);
  EXPECT_UKM(UkmEntry::kVideoFramesDecodedName, kDecodedFrameCount2);
  EXPECT_UKM(UkmEntry::kVideoFramesDroppedName, kDroppedFrameCount2);

  EXPECT_UKM(UkmEntry::kCompletedRebuffersDurationName, 0);
  EXPECT_UKM(UkmEntry::kAudioCodecName, secondary_properties2->audio_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecName, secondary_properties2->video_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecProfileName,
             secondary_properties2->video_codec_profile);
  EXPECT_UKM(
      UkmEntry::kAudioEncryptionSchemeName,
      static_cast<int64_t>(secondary_properties2->audio_encryption_scheme));
  EXPECT_UKM(
      UkmEntry::kVideoEncryptionSchemeName,
      static_cast<int64_t>(secondary_properties2->video_encryption_scheme));
  EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
             secondary_properties2->natural_size.width());
  EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
             secondary_properties2->natural_size.height());
}

TEST_F(WatchTimeRecorderTest, MultipleSecondaryPropertiesNoFinalizeNo2ndWT) {
  mojom::PlaybackPropertiesPtr properties = mojom::PlaybackProperties::New(
      true, true, false, false, true, true, false);
  mojom::SecondaryPlaybackPropertiesPtr secondary_properties1 =
      mojom::SecondaryPlaybackProperties::New(
          kCodecOpus, kCodecVP9, VP9PROFILE_PROFILE0, "MojoAudioDecoder",
          "MojoVideoDecoder", EncryptionScheme::kUnencrypted,
          EncryptionScheme::kUnencrypted, gfx::Size(400, 300));
  Initialize(properties.Clone());
  wtr_->UpdateSecondaryProperties(secondary_properties1.Clone());

  constexpr base::TimeDelta kUnderflowDuration =
      base::TimeDelta::FromMilliseconds(250);
  constexpr base::TimeDelta kWatchTime1 = base::TimeDelta::FromSeconds(54);
  const int kUnderflowCount1 = 2;
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoAll, kWatchTime1);
  wtr_->UpdateUnderflowCount(kUnderflowCount1);
  wtr_->UpdateUnderflowDuration(kUnderflowCount1, kUnderflowDuration);

  constexpr int kDecodedFrameCount1 = 10;
  constexpr int kDroppedFrameCount1 = 2;
  wtr_->UpdateVideoDecodeStats(kDecodedFrameCount1, kDroppedFrameCount1);

  mojom::SecondaryPlaybackPropertiesPtr secondary_properties2 =
      mojom::SecondaryPlaybackProperties::New(
          kCodecAAC, kCodecH264, H264PROFILE_MAIN, "FFmpegAudioDecoder",
          "FFmpegVideoDecoder", EncryptionScheme::kUnencrypted,
          EncryptionScheme::kUnencrypted, gfx::Size(800, 600));
  wtr_->UpdateSecondaryProperties(secondary_properties2.Clone());

  // Don't record any watch time to the new record, it should report zero watch
  // time upon destruction. This ensures there's always a Finalize to prevent
  // UKM was receiving negative values from the previous unfinalized record.
  wtr_.reset();
  base::RunLoop().RunUntilIdle();

  // All records should have the following:
  const auto& entries = test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(2u, entries.size());
  for (const auto* entry : entries) {
    test_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestOrigin));
    EXPECT_UKM(UkmEntry::kIsBackgroundName, properties->is_background);
    EXPECT_UKM(UkmEntry::kIsMutedName, properties->is_muted);
    EXPECT_UKM(UkmEntry::kHasAudioName, properties->has_audio);
    EXPECT_UKM(UkmEntry::kHasVideoName, properties->has_video);
    EXPECT_UKM(UkmEntry::kIsEMEName, properties->is_eme);
    EXPECT_UKM(UkmEntry::kIsMSEName, properties->is_mse);
    EXPECT_UKM(UkmEntry::kAutoplayInitiatedName, false);
    EXPECT_UKM(UkmEntry::kLastPipelineStatusName, PIPELINE_OK);
    EXPECT_HAS_UKM(UkmEntry::kPlayerIDName);
    EXPECT_NO_UKM(UkmEntry::kDurationName);
  }

  // The first record should have...
  auto* entry = entries[0];
  EXPECT_UKM(UkmEntry::kWatchTimeName, kWatchTime1.InMilliseconds());
  EXPECT_UKM(UkmEntry::kMeanTimeBetweenRebuffersName,
             kWatchTime1.InMilliseconds() / kUnderflowCount1);
  EXPECT_UKM(UkmEntry::kAudioDecoderNameName, 2);
  EXPECT_UKM(UkmEntry::kVideoDecoderNameName, 5);
  EXPECT_UKM(UkmEntry::kRebuffersCountName, kUnderflowCount1);
  EXPECT_UKM(UkmEntry::kCompletedRebuffersCountName, kUnderflowCount1);
  EXPECT_UKM(UkmEntry::kCompletedRebuffersDurationName,
             kUnderflowDuration.InMilliseconds());
  EXPECT_UKM(UkmEntry::kVideoFramesDecodedName, kDecodedFrameCount1);
  EXPECT_UKM(UkmEntry::kVideoFramesDroppedName, kDroppedFrameCount1);
  EXPECT_UKM(UkmEntry::kAudioCodecName, secondary_properties1->audio_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecName, secondary_properties1->video_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecProfileName,
             secondary_properties1->video_codec_profile);
  EXPECT_UKM(
      UkmEntry::kAudioEncryptionSchemeName,
      static_cast<int64_t>(secondary_properties1->audio_encryption_scheme));
  EXPECT_UKM(
      UkmEntry::kVideoEncryptionSchemeName,
      static_cast<int64_t>(secondary_properties1->video_encryption_scheme));
  EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
             secondary_properties1->natural_size.width());
  EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
             secondary_properties1->natural_size.height());

  // The second record should have...
  entry = entries[1];
  EXPECT_UKM(UkmEntry::kWatchTimeName, 0);
  EXPECT_UKM(UkmEntry::kAudioDecoderNameName, 1);
  EXPECT_UKM(UkmEntry::kVideoDecoderNameName, 2);
  EXPECT_UKM(UkmEntry::kRebuffersCountName, 0);
  EXPECT_UKM(UkmEntry::kCompletedRebuffersCountName, 0);
  EXPECT_UKM(UkmEntry::kCompletedRebuffersDurationName, 0);
  EXPECT_UKM(UkmEntry::kVideoFramesDecodedName, 0);
  EXPECT_UKM(UkmEntry::kVideoFramesDroppedName, 0);
  EXPECT_UKM(UkmEntry::kAudioCodecName, secondary_properties2->audio_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecName, secondary_properties2->video_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecProfileName,
             secondary_properties2->video_codec_profile);
  EXPECT_UKM(
      UkmEntry::kAudioEncryptionSchemeName,
      static_cast<int64_t>(secondary_properties2->audio_encryption_scheme));
  EXPECT_UKM(
      UkmEntry::kVideoEncryptionSchemeName,
      static_cast<int64_t>(secondary_properties2->video_encryption_scheme));
  EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
             secondary_properties2->natural_size.width());
  EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
             secondary_properties2->natural_size.height());
}

TEST_F(WatchTimeRecorderTest, MultipleSecondaryPropertiesWithFinalize) {
  mojom::PlaybackPropertiesPtr properties = mojom::PlaybackProperties::New(
      true, true, false, false, true, true, false);
  mojom::SecondaryPlaybackPropertiesPtr secondary_properties1 =
      mojom::SecondaryPlaybackProperties::New(
          kCodecOpus, kCodecVP9, VP9PROFILE_PROFILE0, "MojoAudioDecoder",
          "MojoVideoDecoder", EncryptionScheme::kCbcs, EncryptionScheme::kCbcs,
          gfx::Size(400, 300));
  Initialize(properties.Clone());
  wtr_->UpdateSecondaryProperties(secondary_properties1.Clone());

  constexpr base::TimeDelta kUnderflowDuration =
      base::TimeDelta::FromMilliseconds(250);
  constexpr base::TimeDelta kWatchTime1 = base::TimeDelta::FromSeconds(54);
  const int kUnderflowCount1 = 2;
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoAll, kWatchTime1);
  wtr_->UpdateUnderflowCount(kUnderflowCount1);
  wtr_->UpdateUnderflowDuration(kUnderflowCount1, kUnderflowDuration);

  constexpr int kDecodedFrameCount1 = 10;
  constexpr int kDroppedFrameCount1 = 2;
  wtr_->UpdateVideoDecodeStats(kDecodedFrameCount1, kDroppedFrameCount1);

  // Force a finalize here so that the there is no unfinalized watch time at the
  // time of the secondary property update.
  wtr_->FinalizeWatchTime({});

  mojom::SecondaryPlaybackPropertiesPtr secondary_properties2 =
      mojom::SecondaryPlaybackProperties::New(
          kCodecAAC, kCodecH264, H264PROFILE_MAIN, "FFmpegAudioDecoder",
          "FFmpegVideoDecoder", EncryptionScheme::kUnencrypted,
          EncryptionScheme::kUnencrypted, gfx::Size(800, 600));
  wtr_->UpdateSecondaryProperties(secondary_properties2.Clone());

  constexpr base::TimeDelta kWatchTime2 = base::TimeDelta::FromSeconds(25);
  const int kUnderflowCount2 = 3;

  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoAll, kWatchTime2);
  wtr_->UpdateUnderflowCount(kUnderflowCount2);
  wtr_->OnError(PIPELINE_ERROR_DECODE);

  wtr_.reset();
  base::RunLoop().RunUntilIdle();

  // All records should have the following:
  const auto& entries = test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(2u, entries.size());
  for (const auto* entry : entries) {
    test_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestOrigin));
    EXPECT_UKM(UkmEntry::kIsBackgroundName, properties->is_background);
    EXPECT_UKM(UkmEntry::kIsMutedName, properties->is_muted);
    EXPECT_UKM(UkmEntry::kHasAudioName, properties->has_audio);
    EXPECT_UKM(UkmEntry::kHasVideoName, properties->has_video);
    EXPECT_UKM(UkmEntry::kIsEMEName, properties->is_eme);
    EXPECT_UKM(UkmEntry::kIsMSEName, properties->is_mse);
    EXPECT_UKM(UkmEntry::kAutoplayInitiatedName, false);
    EXPECT_HAS_UKM(UkmEntry::kPlayerIDName);
    EXPECT_NO_UKM(UkmEntry::kDurationName);

    // All records inherit the final pipeline status code.
    EXPECT_UKM(UkmEntry::kLastPipelineStatusName, PIPELINE_ERROR_DECODE);
  }

  // The first record should have...
  auto* entry = entries[0];
  EXPECT_UKM(UkmEntry::kWatchTimeName, kWatchTime1.InMilliseconds());
  EXPECT_UKM(UkmEntry::kMeanTimeBetweenRebuffersName,
             kWatchTime1.InMilliseconds() / kUnderflowCount1);
  EXPECT_UKM(UkmEntry::kAudioDecoderNameName, 2);
  EXPECT_UKM(UkmEntry::kVideoDecoderNameName, 5);
  EXPECT_UKM(UkmEntry::kRebuffersCountName, kUnderflowCount1);
  EXPECT_UKM(UkmEntry::kCompletedRebuffersCountName, kUnderflowCount1);
  EXPECT_UKM(UkmEntry::kCompletedRebuffersDurationName,
             kUnderflowDuration.InMilliseconds());
  EXPECT_UKM(UkmEntry::kVideoFramesDecodedName, kDecodedFrameCount1);
  EXPECT_UKM(UkmEntry::kVideoFramesDroppedName, kDroppedFrameCount1);
  EXPECT_UKM(UkmEntry::kAudioCodecName, secondary_properties1->audio_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecName, secondary_properties1->video_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecProfileName,
             secondary_properties1->video_codec_profile);
  EXPECT_UKM(
      UkmEntry::kAudioEncryptionSchemeName,
      static_cast<int64_t>(secondary_properties1->audio_encryption_scheme));
  EXPECT_UKM(
      UkmEntry::kVideoEncryptionSchemeName,
      static_cast<int64_t>(secondary_properties1->video_encryption_scheme));
  EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
             secondary_properties1->natural_size.width());
  EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
             secondary_properties1->natural_size.height());

  // The second record should have...
  entry = entries[1];
  EXPECT_UKM(UkmEntry::kWatchTimeName, kWatchTime2.InMilliseconds());
  EXPECT_UKM(UkmEntry::kMeanTimeBetweenRebuffersName,
             kWatchTime2.InMilliseconds() / kUnderflowCount2);
  EXPECT_UKM(UkmEntry::kAudioDecoderNameName, 1);
  EXPECT_UKM(UkmEntry::kVideoDecoderNameName, 2);
  EXPECT_UKM(UkmEntry::kRebuffersCountName, kUnderflowCount2);
  EXPECT_UKM(UkmEntry::kCompletedRebuffersCountName, 0);
  EXPECT_UKM(UkmEntry::kCompletedRebuffersDurationName, 0);
  EXPECT_UKM(UkmEntry::kVideoFramesDecodedName, 0);
  EXPECT_UKM(UkmEntry::kVideoFramesDroppedName, 0);
  EXPECT_UKM(UkmEntry::kAudioCodecName, secondary_properties2->audio_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecName, secondary_properties2->video_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecProfileName,
             secondary_properties2->video_codec_profile);
  EXPECT_UKM(
      UkmEntry::kAudioEncryptionSchemeName,
      static_cast<int64_t>(secondary_properties2->audio_encryption_scheme));
  EXPECT_UKM(
      UkmEntry::kVideoEncryptionSchemeName,
      static_cast<int64_t>(secondary_properties2->video_encryption_scheme));
  EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
             secondary_properties2->natural_size.width());
  EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
             secondary_properties2->natural_size.height());
}

TEST_F(WatchTimeRecorderTest, MultipleSecondaryPropertiesRebufferCarryover) {
  mojom::PlaybackPropertiesPtr properties = mojom::PlaybackProperties::New(
      true, true, false, false, true, true, false);
  mojom::SecondaryPlaybackPropertiesPtr secondary_properties1 =
      mojom::SecondaryPlaybackProperties::New(
          kCodecOpus, kCodecVP9, VP9PROFILE_PROFILE0, "MojoAudioDecoder",
          "MojoVideoDecoder", EncryptionScheme::kCbcs, EncryptionScheme::kCbcs,
          gfx::Size(400, 300));
  Initialize(properties.Clone());
  wtr_->UpdateSecondaryProperties(secondary_properties1.Clone());

  constexpr base::TimeDelta kUnderflowDuration =
      base::TimeDelta::FromMilliseconds(250);
  constexpr base::TimeDelta kWatchTime1 = base::TimeDelta::FromSeconds(54);
  const int kUnderflowCount1 = 2;
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoAll, kWatchTime1);
  wtr_->UpdateUnderflowCount(kUnderflowCount1);

  // Complete all but one of the rebuffers in this update.
  wtr_->UpdateUnderflowDuration(kUnderflowCount1 - 1, kUnderflowDuration);

  mojom::SecondaryPlaybackPropertiesPtr secondary_properties2 =
      mojom::SecondaryPlaybackProperties::New(
          kCodecAAC, kCodecH264, H264PROFILE_MAIN, "FFmpegAudioDecoder",
          "FFmpegVideoDecoder", EncryptionScheme::kUnencrypted,
          EncryptionScheme::kUnencrypted, gfx::Size(800, 600));
  wtr_->UpdateSecondaryProperties(secondary_properties2.Clone());

  constexpr base::TimeDelta kWatchTime2 = base::TimeDelta::FromSeconds(25);
  const int kUnderflowCount2 = 3;

  // Watch time and underflow counts continue to accumulate during property
  // changes, so we report the sum here instead of just kWatchTime2.
  wtr_->RecordWatchTime(WatchTimeKey::kAudioVideoAll,
                        kWatchTime1 + kWatchTime2);
  wtr_->UpdateUnderflowCount(kUnderflowCount1 + kUnderflowCount2);

  // Complete the last underflow in the new property set. Unfortunately this
  // means it will now be associated with this block of watch time. Use a non
  // integer multiplier to avoid incorrect carry over being hidden.
  wtr_->UpdateUnderflowDuration(kUnderflowCount1, kUnderflowDuration * 1.5);

  wtr_->OnError(PIPELINE_ERROR_DECODE);
  wtr_->OnDurationChanged(base::TimeDelta::FromSeconds(5125));

  wtr_.reset();
  base::RunLoop().RunUntilIdle();

  // All records should have the following:
  const auto& entries = test_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(2u, entries.size());
  for (const auto* entry : entries) {
    test_recorder_->ExpectEntrySourceHasUrl(entry, GURL(kTestOrigin));
    EXPECT_UKM(UkmEntry::kIsBackgroundName, properties->is_background);
    EXPECT_UKM(UkmEntry::kIsMutedName, properties->is_muted);
    EXPECT_UKM(UkmEntry::kHasAudioName, properties->has_audio);
    EXPECT_UKM(UkmEntry::kHasVideoName, properties->has_video);
    EXPECT_UKM(UkmEntry::kIsEMEName, properties->is_eme);
    EXPECT_UKM(UkmEntry::kIsMSEName, properties->is_mse);
    EXPECT_UKM(UkmEntry::kAutoplayInitiatedName, false);
    EXPECT_UKM(UkmEntry::kDurationName, 5000000);
    EXPECT_HAS_UKM(UkmEntry::kPlayerIDName);

    // All records inherit the final pipeline status code.
    EXPECT_UKM(UkmEntry::kLastPipelineStatusName, PIPELINE_ERROR_DECODE);
  }

  // The first record should have...
  auto* entry = entries[0];
  EXPECT_UKM(UkmEntry::kWatchTimeName, kWatchTime1.InMilliseconds());
  EXPECT_UKM(UkmEntry::kMeanTimeBetweenRebuffersName,
             kWatchTime1.InMilliseconds() / kUnderflowCount1);
  EXPECT_UKM(UkmEntry::kAudioDecoderNameName, 2);
  EXPECT_UKM(UkmEntry::kVideoDecoderNameName, 5);
  EXPECT_UKM(UkmEntry::kRebuffersCountName, kUnderflowCount1);
  EXPECT_UKM(UkmEntry::kCompletedRebuffersCountName, kUnderflowCount1 - 1);
  EXPECT_UKM(UkmEntry::kCompletedRebuffersDurationName,
             kUnderflowDuration.InMilliseconds());
  EXPECT_UKM(UkmEntry::kAudioCodecName, secondary_properties1->audio_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecName, secondary_properties1->video_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecProfileName,
             secondary_properties1->video_codec_profile);
  EXPECT_UKM(
      UkmEntry::kAudioEncryptionSchemeName,
      static_cast<int64_t>(secondary_properties1->audio_encryption_scheme));
  EXPECT_UKM(
      UkmEntry::kVideoEncryptionSchemeName,
      static_cast<int64_t>(secondary_properties1->video_encryption_scheme));
  EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
             secondary_properties1->natural_size.width());
  EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
             secondary_properties1->natural_size.height());

  // The second record should have...
  entry = entries[1];
  EXPECT_UKM(UkmEntry::kWatchTimeName, kWatchTime2.InMilliseconds());
  EXPECT_UKM(UkmEntry::kMeanTimeBetweenRebuffersName,
             kWatchTime2.InMilliseconds() / kUnderflowCount2);
  EXPECT_UKM(UkmEntry::kAudioDecoderNameName, 1);
  EXPECT_UKM(UkmEntry::kVideoDecoderNameName, 2);
  EXPECT_UKM(UkmEntry::kRebuffersCountName, kUnderflowCount2);
  EXPECT_UKM(UkmEntry::kCompletedRebuffersCountName, 1);
  EXPECT_UKM(UkmEntry::kCompletedRebuffersDurationName,
             (kUnderflowDuration * 1.5 - kUnderflowDuration).InMilliseconds());
  EXPECT_UKM(UkmEntry::kAudioCodecName, secondary_properties2->audio_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecName, secondary_properties2->video_codec);
  EXPECT_UKM(UkmEntry::kVideoCodecProfileName,
             secondary_properties2->video_codec_profile);
  EXPECT_UKM(
      UkmEntry::kAudioEncryptionSchemeName,
      static_cast<int64_t>(secondary_properties2->audio_encryption_scheme));
  EXPECT_UKM(
      UkmEntry::kVideoEncryptionSchemeName,
      static_cast<int64_t>(secondary_properties2->video_encryption_scheme));
  EXPECT_UKM(UkmEntry::kVideoNaturalWidthName,
             secondary_properties2->natural_size.width());
  EXPECT_UKM(UkmEntry::kVideoNaturalHeightName,
             secondary_properties2->natural_size.height());
}

#undef EXPECT_UKM
#undef EXPECT_NO_UKM
#undef EXPECT_HAS_UKM

TEST_F(WatchTimeRecorderTest, DISABLED_PrintExpectedDecoderNameHashes) {
  const std::string kDecoderNames[] = {
      "FFmpegAudioDecoder", "FFmpegVideoDecoder",     "GpuVideoDecoder",
      "MojoVideoDecoder",   "MojoAudioDecoder",       "VpxVideoDecoder",
      "AomVideoDecoder",    "DecryptingAudioDecoder", "DecryptingVideoDecoder",
      "Dav1dVideoDecoder",  "FuchsiaVideoDecoder",    "MediaPlayer"};
  printf("%18s = 0\n", "None");
  for (const auto& name : kDecoderNames)
    printf("%18s = 0x%x\n", name.c_str(), base::PersistentHash(name));
}

}  // namespace media
