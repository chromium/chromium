// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/watch_time_recorder.h"

#include <cmath>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "media/base/limits.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder.h"
#include "media/base/watch_time_keys.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace media {

// The minimum amount of media playback which can elapse before we'll report
// watch time metrics for a playback.
constexpr base::TimeDelta kMinimumElapsedWatchTime =
    base::Seconds(limits::kMinimumElapsedWatchTimeSecs);

static void RecordWatchTimeInternal(
    std::string_view key,
    base::TimeDelta value,
    base::TimeDelta minimum = kMinimumElapsedWatchTime) {
  DCHECK(!key.empty());
  base::UmaHistogramCustomTimes(key, value, minimum, base::Hours(10), 50);
}

static void RecordMeanTimeBetweenRebuffers(std::string_view key,
                                           base::TimeDelta value) {
  DCHECK(!key.empty());

  // There are a maximum of 5 underflow events possible in a given 7s watch time
  // period, so the minimum value is 1.4s.
  RecordWatchTimeInternal(key, value, base::Seconds(1.4));
}

static void RecordDiscardedWatchTime(std::string_view key,
                                     base::TimeDelta value) {
  DCHECK(!key.empty());
  base::UmaHistogramCustomTimes(key, value, base::TimeDelta(),
                                kMinimumElapsedWatchTime, 50);
}

static void RecordRebuffersCount(std::string_view key, int underflow_count) {
  DCHECK(!key.empty());
  base::UmaHistogramCounts100(key, underflow_count);
}

WatchTimeRecorder::WatchTimeUkmRecord::WatchTimeUkmRecord(
    mojom::SecondaryPlaybackPropertiesPtr properties)
    : secondary_properties(std::move(properties)) {}

WatchTimeRecorder::WatchTimeUkmRecord::WatchTimeUkmRecord(
    WatchTimeUkmRecord&& record) = default;

WatchTimeRecorder::WatchTimeUkmRecord::~WatchTimeUkmRecord() = default;

WatchTimeRecorder::WatchTimeRecorder(mojom::PlaybackPropertiesPtr properties,
                                     ukm::SourceId source_id,
                                     bool is_top_frame,
                                     uint64_t player_id)
    : properties_(std::move(properties)),
      source_id_(source_id),
      is_top_frame_(is_top_frame),
      player_id_(player_id),
      extended_metrics_keys_(
          {{WatchTimeKey::kAudioSrc, kMeanTimeBetweenRebuffersAudioSrc,
            kRebuffersCountAudioSrc, kDiscardedWatchTimeAudioSrc},
           {WatchTimeKey::kAudioMse, kMeanTimeBetweenRebuffersAudioMse,
            kRebuffersCountAudioMse, kDiscardedWatchTimeAudioMse},
           {WatchTimeKey::kAudioEme, kMeanTimeBetweenRebuffersAudioEme,
            kRebuffersCountAudioEme, kDiscardedWatchTimeAudioEme},
           {WatchTimeKey::kAudioVideoSrc,
            kMeanTimeBetweenRebuffersAudioVideoSrc,
            kRebuffersCountAudioVideoSrc, kDiscardedWatchTimeAudioVideoSrc},
           {WatchTimeKey::kAudioVideoMse,
            kMeanTimeBetweenRebuffersAudioVideoMse,
            kRebuffersCountAudioVideoMse, kDiscardedWatchTimeAudioVideoMse},
           {WatchTimeKey::kAudioVideoEme,
            kMeanTimeBetweenRebuffersAudioVideoEme,
            kRebuffersCountAudioVideoEme, kDiscardedWatchTimeAudioVideoEme}}) {}

WatchTimeRecorder::~WatchTimeRecorder() {
  FinalizeWatchTime({});
  RecordUkmPlaybackData();
}

void WatchTimeRecorder::RecordWatchTime(WatchTimeKey key,
                                        base::TimeDelta watch_time) {
  watch_time_info_[key] = watch_time;
}

void WatchTimeRecorder::FinalizeWatchTime(
    const std::vector<WatchTimeKey>& keys_to_finalize) {
  // If the filter set is empty, treat that as finalizing all keys; otherwise
  // only the listed keys will be finalized.
  const bool should_finalize_everything = keys_to_finalize.empty();

  // Record metrics to be finalized, but do not erase them yet; they are still
  // needed by for UKM and MTBR recording below.
  for (auto& kv : watch_time_info_) {
    if (!should_finalize_everything &&
        !base::Contains(keys_to_finalize, kv.first)) {
      continue;
    }

    // Report only certain keys to UMA and only if they have at met the minimum
    // watch time requirement. Otherwise, for SRC/MSE/EME keys, log them to the
    // discard metric.
    std::string_view key_str = ConvertWatchTimeKeyToStringForUma(kv.first);
    if (ShouldRecordUma() && !key_str.empty()) {
      if (kv.second >= kMinimumElapsedWatchTime) {
        RecordWatchTimeInternal(key_str, kv.second);
      } else if (kv.second.is_positive()) {
        auto it = base::ranges::find(extended_metrics_keys_, kv.first,
                                     &ExtendedMetricsKeyMap::watch_time_key);
        if (it != extended_metrics_keys_.end())
          RecordDiscardedWatchTime(it->discard_key, kv.second);
      }
    }

    // At finalize, update the aggregate entry.
    if (!ukm_records_.empty())
      ukm_records_.back().aggregate_watch_time_info[kv.first] += kv.second;
  }

  // If we're not finalizing everything, we're done after removing keys.
  if (!should_finalize_everything) {
    for (auto key : keys_to_finalize)
      watch_time_info_.erase(key);
    return;
  }

  // Check for watch times entries that have corresponding MTBR entries and
  // report the MTBR value using watch_time / |underflow_count|. Do this only
  // for foreground reporters since we only have UMA keys for foreground.
  if (ShouldRecordUma() && !properties_->is_background &&
      !properties_->is_muted) {
    for (auto& mapping : extended_metrics_keys_) {
      auto it = watch_time_info_.find(mapping.watch_time_key);
      if (it == watch_time_info_.end() || it->second < kMinimumElapsedWatchTime)
        continue;

      if (underflow_count_) {
        RecordMeanTimeBetweenRebuffers(mapping.mtbr_key,
                                       it->second / underflow_count_);
      }

      RecordRebuffersCount(mapping.smooth_rate_key, underflow_count_);
    }
  }

  // Ensure values are cleared in case the reporter is reused.
  if (!ukm_records_.empty()) {
    auto& last_record = ukm_records_.back();
    last_record.total_underflow_count += underflow_count_;
    last_record.total_completed_underflow_count += completed_underflow_count_;
    last_record.total_underflow_duration += underflow_duration_;
    last_record.total_video_frames_decoded += video_frames_decoded_;
    last_record.total_video_frames_dropped += video_frames_dropped_;
  }

  video_frames_decoded_ = video_frames_dropped_ = 0;
  underflow_count_ = completed_underflow_count_ = 0;
  underflow_duration_ = base::TimeDelta();
  watch_time_info_.clear();
}

void WatchTimeRecorder::OnError(const PipelineStatus& status) {
  pipeline_status_ = status.code();
}

void WatchTimeRecorder::UpdateSecondaryProperties(
    mojom::SecondaryPlaybackPropertiesPtr secondary_properties) {
  bool last_record_was_unfinalized = false;
  if (!ukm_records_.empty()) {
    auto& last_record = ukm_records_.back();

    // Skip unchanged property updates.
    if (secondary_properties->Equals(*last_record.secondary_properties))
      return;

    // If a property just changes from an unknown to a known value, allow the
    // update without creating a whole new record. Not checking
    // audio_encryption_scheme and video_encryption_scheme as we want to
    // capture changes in encryption schemes.
    if (last_record.secondary_properties->audio_codec == AudioCodec::kUnknown ||
        last_record.secondary_properties->video_codec == VideoCodec::kUnknown ||
        last_record.secondary_properties->audio_codec_profile ==
            AudioCodecProfile::kUnknown ||
        last_record.secondary_properties->video_codec_profile ==
            VIDEO_CODEC_PROFILE_UNKNOWN ||
        last_record.secondary_properties->audio_decoder ==
            AudioDecoderType::kUnknown ||
        last_record.secondary_properties->video_decoder ==
            VideoDecoderType::kUnknown) {
      auto temp_props = last_record.secondary_properties.Clone();
      if (last_record.secondary_properties->audio_codec == AudioCodec::kUnknown)
        temp_props->audio_codec = secondary_properties->audio_codec;
      if (last_record.secondary_properties->video_codec == VideoCodec::kUnknown)
        temp_props->video_codec = secondary_properties->video_codec;
      if (last_record.secondary_properties->audio_codec_profile ==
          AudioCodecProfile::kUnknown) {
        temp_props->audio_codec_profile =
            secondary_properties->audio_codec_profile;
      }
      if (last_record.secondary_properties->video_codec_profile ==
          VIDEO_CODEC_PROFILE_UNKNOWN) {
        temp_props->video_codec_profile =
            secondary_properties->video_codec_profile;
      }
      if (last_record.secondary_properties->audio_decoder ==
          AudioDecoderType::kUnknown) {
        temp_props->audio_decoder = secondary_properties->audio_decoder;
      }
      if (last_record.secondary_properties->video_decoder ==
          VideoDecoderType::kUnknown) {
        temp_props->video_decoder = secondary_properties->video_decoder;
      }
      if (temp_props->Equals(*secondary_properties)) {
        last_record.secondary_properties = std::move(temp_props);
        return;
      }
    }

    // Flush any existing watch time for the current UKM record. The client is
    // responsible for ensuring recent watch time has been reported before
    // updating the secondary properties.
    for (auto& kv : watch_time_info_)
      last_record.aggregate_watch_time_info[kv.first] += kv.second;
    last_record.total_underflow_count += underflow_count_;
    last_record.total_completed_underflow_count += completed_underflow_count_;
    last_record.total_underflow_duration += underflow_duration_;
    last_record.total_video_frames_decoded += video_frames_decoded_;
    last_record.total_video_frames_dropped += video_frames_dropped_;

    // If we flushed any watch time or underflow counts which hadn't been
    // finalized we'll need to ensure the eventual Finalize() correctly accounts
    // for those values at the time of the secondary property update.
    last_record_was_unfinalized =
        !watch_time_info_.empty() || underflow_count_ ||
        completed_underflow_count_ || !underflow_duration_.is_zero() ||
        video_frames_decoded_ || video_frames_dropped_;
  }
  ukm_records_.emplace_back(std::move(secondary_properties));

  // We're still in the middle of ongoing watch time updates. So offset the
  // future records by their current values; this is done by setting the initial
  // value of each unfinalized record to the negative of its current value.
  //
  // These values will be made positive by the next Finalize() call; which is
  // guaranteed to be called at least one more time; either at destruction or by
  // the client. This ensures we report the correct amount of watch time that
  // has elapsed since the secondary properties were updated.
  //
  // E.g., consider the case where there's a pending watch time entry for
  // kAudioAll=10s and the next RecordWatchTime() call would be kAudioAll=25s.
  // Without offsetting, if UpdateSecondaryProperties() is called before the
  // next RecordWatchTime() we'll end up recording kAudioAll=25s as the amount
  // of watch time for the new set of secondary properties, which isn't correct.
  // We instead want to report kAudioAll = 25s - 10s = 15s.
  if (last_record_was_unfinalized) {
    auto& last_record = ukm_records_.back();
    last_record.total_underflow_count = -underflow_count_;
    last_record.total_completed_underflow_count = -completed_underflow_count_;
    last_record.total_underflow_duration = -underflow_duration_;
    last_record.total_video_frames_decoded = -video_frames_decoded_;
    last_record.total_video_frames_dropped = -video_frames_dropped_;
    for (auto& kv : watch_time_info_)
      last_record.aggregate_watch_time_info[kv.first] = -kv.second;
  }
}

void WatchTimeRecorder::SetAutoplayInitiated(bool value) {
  DCHECK(!autoplay_initiated_.has_value() || value == autoplay_initiated_);
  autoplay_initiated_ = value;
}

void WatchTimeRecorder::OnDurationChanged(base::TimeDelta duration) {
  duration_ = duration;
}

void WatchTimeRecorder::UpdateVideoDecodeStats(uint32_t video_frames_decoded,
                                               uint32_t video_frames_dropped) {
  video_frames_decoded_ = video_frames_decoded;
  video_frames_dropped_ = video_frames_dropped;
}

void WatchTimeRecorder::UpdateUnderflowCount(int32_t total_count) {
  underflow_count_ = total_count;
}

void WatchTimeRecorder::UpdateUnderflowDuration(
    int32_t total_completed_count,
    base::TimeDelta total_duration) {
  completed_underflow_count_ = total_completed_count;
  underflow_duration_ = total_duration;
}

void WatchTimeRecorder::RecordUkmPlaybackData() {
  // UKM may be unavailable in content_shell or other non-chrome/ builds; it
  // may also be unavailable if browser shutdown has started; so this may be a
  // nullptr. If it's unavailable, UKM reporting will be skipped.
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  // Round duration to the most significant digit in milliseconds for privacy.
  std::optional<uint64_t> clamped_duration_ms;
  if (duration_ != kNoTimestamp && duration_ != kInfiniteDuration) {
    clamped_duration_ms = duration_.InMilliseconds();
    if (duration_ > base::Seconds(1)) {
      // Turns 54321 => 10000.
      const uint64_t base =
          std::pow(10, static_cast<uint64_t>(std::log10(*clamped_duration_ms)));
      // Turns 54321 => 4321.
      const uint64_t modulus = *clamped_duration_ms % base;
      // Turns 54321 => 50000 and 55321 => 60000
      clamped_duration_ms =
          *clamped_duration_ms - modulus + (modulus < base / 2 ? 0 : base);
    }
  }

  base::flat_set<AudioCodecProfile> aac_profiles;

  for (auto& ukm_record : ukm_records_) {
    ukm::builders::Media_BasicPlayback builder(source_id_);

    builder.SetIsTopFrame(is_top_frame_);
    builder.SetIsBackground(properties_->is_background);
    builder.SetIsMuted(properties_->is_muted);
    builder.SetPlayerID(player_id_);
    if (clamped_duration_ms.has_value())
      builder.SetDuration(*clamped_duration_ms);

    bool recorded_all_metric = false;
    for (auto& kv : ukm_record.aggregate_watch_time_info) {
      DCHECK_GE(kv.second, base::TimeDelta());

      if (kv.first == WatchTimeKey::kAudioAll ||
          kv.first == WatchTimeKey::kAudioBackgroundAll ||
          kv.first == WatchTimeKey::kAudioVideoAll ||
          kv.first == WatchTimeKey::kAudioVideoMutedAll ||
          kv.first == WatchTimeKey::kAudioVideoBackgroundAll ||
          kv.first == WatchTimeKey::kVideoAll ||
          kv.first == WatchTimeKey::kVideoBackgroundAll) {
        // Only one of these keys should be present.
        DCHECK(!recorded_all_metric);
        recorded_all_metric = true;

        builder.SetWatchTime(kv.second.InMilliseconds());
        if (ukm_record.total_underflow_count) {
          builder.SetMeanTimeBetweenRebuffers(
              (kv.second / ukm_record.total_underflow_count).InMilliseconds());
        }
      } else if (kv.first == WatchTimeKey::kAudioAc ||
                 kv.first == WatchTimeKey::kAudioBackgroundAc ||
                 kv.first == WatchTimeKey::kAudioVideoAc ||
                 kv.first == WatchTimeKey::kAudioVideoMutedAc ||
                 kv.first == WatchTimeKey::kAudioVideoBackgroundAc ||
                 kv.first == WatchTimeKey::kVideoAc ||
                 kv.first == WatchTimeKey::kVideoBackgroundAc) {
        builder.SetWatchTime_AC(kv.second.InMilliseconds());
      } else if (kv.first == WatchTimeKey::kAudioBattery ||
                 kv.first == WatchTimeKey::kAudioBackgroundBattery ||
                 kv.first == WatchTimeKey::kAudioVideoBattery ||
                 kv.first == WatchTimeKey::kAudioVideoMutedBattery ||
                 kv.first == WatchTimeKey::kAudioVideoBackgroundBattery ||
                 kv.first == WatchTimeKey::kVideoBattery ||
                 kv.first == WatchTimeKey::kVideoBackgroundBattery) {
        builder.SetWatchTime_Battery(kv.second.InMilliseconds());
      } else if (kv.first == WatchTimeKey::kAudioNativeControlsOn ||
                 kv.first == WatchTimeKey::kAudioVideoNativeControlsOn ||
                 kv.first == WatchTimeKey::kAudioVideoMutedNativeControlsOn ||
                 kv.first == WatchTimeKey::kVideoNativeControlsOn) {
        builder.SetWatchTime_NativeControlsOn(kv.second.InMilliseconds());
      } else if (kv.first == WatchTimeKey::kAudioNativeControlsOff ||
                 kv.first == WatchTimeKey::kAudioVideoNativeControlsOff ||
                 kv.first == WatchTimeKey::kAudioVideoMutedNativeControlsOff ||
                 kv.first == WatchTimeKey::kVideoNativeControlsOff) {
        builder.SetWatchTime_NativeControlsOff(kv.second.InMilliseconds());
      } else if (kv.first == WatchTimeKey::kAudioVideoDisplayFullscreen ||
                 kv.first == WatchTimeKey::kAudioVideoMutedDisplayFullscreen ||
                 kv.first == WatchTimeKey::kVideoDisplayFullscreen) {
        builder.SetWatchTime_DisplayFullscreen(kv.second.InMilliseconds());
      } else if (kv.first == WatchTimeKey::kAudioVideoDisplayInline ||
                 kv.first == WatchTimeKey::kAudioVideoMutedDisplayInline ||
                 kv.first == WatchTimeKey::kVideoDisplayInline) {
        builder.SetWatchTime_DisplayInline(kv.second.InMilliseconds());
      } else if (kv.first == WatchTimeKey::kAudioVideoDisplayPictureInPicture ||
                 kv.first ==
                     WatchTimeKey::kAudioVideoMutedDisplayPictureInPicture ||
                 kv.first == WatchTimeKey::kVideoDisplayPictureInPicture) {
        builder.SetWatchTime_DisplayPictureInPicture(
            kv.second.InMilliseconds());
      }
    }

    // See note in mojom::PlaybackProperties about why we have both of these.
    builder.SetAudioCodec(
        static_cast<int64_t>(ukm_record.secondary_properties->audio_codec));
    builder.SetVideoCodec(
        static_cast<int64_t>(ukm_record.secondary_properties->video_codec));
    builder.SetAudioCodecProfile(static_cast<int64_t>(
        ukm_record.secondary_properties->audio_codec_profile));
    builder.SetVideoCodecProfile(
        ukm_record.secondary_properties->video_codec_profile);
    builder.SetHasAudio(properties_->has_audio);
    builder.SetHasVideo(properties_->has_video);

    if (ukm_record.secondary_properties->audio_codec == AudioCodec::kAAC)
      aac_profiles.insert(ukm_record.secondary_properties->audio_codec_profile);

    builder.SetAudioDecoderName(
        static_cast<int64_t>(ukm_record.secondary_properties->audio_decoder));
    builder.SetVideoDecoderName(
        static_cast<int64_t>(ukm_record.secondary_properties->video_decoder));
    builder.SetAudioEncryptionScheme(static_cast<int64_t>(
        ukm_record.secondary_properties->audio_encryption_scheme));
    builder.SetVideoEncryptionScheme(static_cast<int64_t>(
        ukm_record.secondary_properties->video_encryption_scheme));
    builder.SetIsEME(properties_->is_eme);
    builder.SetIsMSE(properties_->is_mse);
    builder.SetMediaStreamType(
        static_cast<int64_t>(properties_->media_stream_type));
    builder.SetLastPipelineStatus(pipeline_status_);
    builder.SetRebuffersCount(ukm_record.total_underflow_count);
    builder.SetCompletedRebuffersCount(
        ukm_record.total_completed_underflow_count);
    builder.SetCompletedRebuffersDuration(
        ukm_record.total_underflow_duration.InMilliseconds());
    builder.SetVideoFramesDecoded(ukm_record.total_video_frames_decoded);
    builder.SetVideoFramesDropped(ukm_record.total_video_frames_dropped);
    builder.SetVideoNaturalWidth(
        ukm_record.secondary_properties->natural_size.width());
    builder.SetVideoNaturalHeight(
        ukm_record.secondary_properties->natural_size.height());
    builder.SetAutoplayInitiated(autoplay_initiated_.value_or(false));
    builder.Record(ukm_recorder);
  }

  if (ShouldRecordUma() && !aac_profiles.empty()) {
    for (auto profile : aac_profiles)
      base::UmaHistogramEnumeration("Media.AudioCodecProfile.AAC", profile);
  }

  ukm_records_.clear();
}

bool WatchTimeRecorder::ShouldRecordUma() const {
  return properties_->media_stream_type == mojom::MediaStreamType::kNone;
}

WatchTimeRecorder::ExtendedMetricsKeyMap::ExtendedMetricsKeyMap(
    const ExtendedMetricsKeyMap& copy)
    : ExtendedMetricsKeyMap(copy.watch_time_key,
                            copy.mtbr_key,
                            copy.smooth_rate_key,
                            copy.discard_key) {}

WatchTimeRecorder::ExtendedMetricsKeyMap::ExtendedMetricsKeyMap(
    WatchTimeKey watch_time_key,
    std::string_view mtbr_key,
    std::string_view smooth_rate_key,
    std::string_view discard_key)
    : watch_time_key(watch_time_key),
      mtbr_key(mtbr_key),
      smooth_rate_key(smooth_rate_key),
      discard_key(discard_key) {}

}  // namespace media
