// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WATCH_TIME_KEYS_H_
#define MEDIA_BASE_WATCH_TIME_KEYS_H_

#include <string_view>

#include "media/base/media_export.h"

namespace media {

enum class WatchTimeKey : int {
  kAudioAll = 0,
  kAudioAutoPipMediaPlayback,
  kAudioMse,
  kAudioEme,
  kAudioSrc,
  kAudioHls,
  kAudioBattery,
  kAudioAc,
  kAudioEmbeddedExperience,
  kAudioNativeControlsOn,
  kAudioNativeControlsOff,
  kAudioBackgroundAll,
  kAudioBackgroundMse,
  kAudioBackgroundEme,
  kAudioBackgroundSrc,
  kAudioBackgroundHls,
  kAudioBackgroundBattery,
  kAudioBackgroundAc,
  kAudioBackgroundEmbeddedExperience,
  kAudioDisplayFullscreen,
  kAudioDisplayInline,
  kAudioDisplayPictureInPicture,
  kAudioVideoAll,
  kAudioVideoAutoPipMediaPlayback,
  kAudioVideoMse,
  kAudioVideoEme,
  kAudioVideoSrc,
  kAudioVideoHls,
  kAudioVideoBattery,
  kAudioVideoAc,
  kAudioVideoDisplayFullscreen,
  kAudioVideoDisplayInline,
  kAudioVideoDisplayPictureInPicture,
  kAudioVideoDominantVisibleContent,
  kAudioVideoAuxiliaryVisibleContent,
  kAudioVideoEmbeddedExperience,
  kAudioVideoNativeControlsOn,
  kAudioVideoNativeControlsOff,
  kAudioVideoBackgroundAll,
  kAudioVideoBackgroundMse,
  kAudioVideoBackgroundEme,
  kAudioVideoBackgroundSrc,
  kAudioVideoBackgroundHls,
  kAudioVideoBackgroundBattery,
  kAudioVideoBackgroundAc,
  kAudioVideoBackgroundEmbeddedExperience,
  kAudioVideoHdrAll,
  kAudioVideoHdrEme,
  kAudioVideoMutedAll,
  kAudioVideoMutedMse,
  kAudioVideoMutedEme,
  kAudioVideoMutedSrc,
  kAudioVideoMutedHls,
  kAudioVideoMutedBattery,
  kAudioVideoMutedAc,
  kAudioVideoMutedEmbeddedExperience,
  kAudioVideoMutedDisplayFullscreen,
  kAudioVideoMutedDisplayInline,
  kAudioVideoMutedDisplayPictureInPicture,
  kAudioVideoMutedDominantVisibleContent,
  kAudioVideoMutedAuxiliaryVisibleContent,
  kAudioVideoMutedNativeControlsOn,
  kAudioVideoMutedNativeControlsOff,
  kAudioVideoMediaFoundationAll,
  kAudioVideoMediaFoundationEme,
  kAudioVideoMediaFoundationHdrAll,
  kAudioVideoMediaFoundationHdrEme,
  kAudioVideoMediaFoundationSdrAll,
  kAudioVideoMediaFoundationSdrEme,
  kAudioVideoSdrAll,
  kAudioVideoSdrEme,
  kVideoAll,
  kVideoMse,
  kVideoEme,
  kVideoSrc,
  kVideoHls,
  kVideoBattery,
  kVideoAc,
  kVideoDisplayFullscreen,
  kVideoDisplayInline,
  kVideoDisplayPictureInPicture,
  kVideoDominantVisibleContent,
  kVideoAuxiliaryVisibleContent,
  kVideoEmbeddedExperience,
  kVideoNativeControlsOn,
  kVideoNativeControlsOff,
  kVideoBackgroundAll,
  kVideoBackgroundMse,
  kVideoBackgroundEme,
  kVideoBackgroundSrc,
  kVideoBackgroundHls,
  kVideoBackgroundBattery,
  kVideoBackgroundAc,
  kVideoBackgroundEmbeddedExperience,
  kWatchTimeKeyMax = kVideoBackgroundEmbeddedExperience
};

// Count of the number of underflow events during a media session.
inline constexpr char kWatchTimeUnderflowCount[] = "UnderflowCount";

// UMA keys for MTBR samples.
inline constexpr std::string_view kMeanTimeBetweenRebuffersAudioSrc =
    "Media.MeanTimeBetweenRebuffers.Audio.SRC";
inline constexpr std::string_view kMeanTimeBetweenRebuffersAudioMse =
    "Media.MeanTimeBetweenRebuffers.Audio.MSE";
inline constexpr std::string_view kMeanTimeBetweenRebuffersAudioEme =
    "Media.MeanTimeBetweenRebuffers.Audio.EME";
inline constexpr std::string_view kMeanTimeBetweenRebuffersAudioHls =
    "Media.MeanTimeBetweenRebuffers.Audio.HLS";
inline constexpr std::string_view kMeanTimeBetweenRebuffersAudioVideoSrc =
    "Media.MeanTimeBetweenRebuffers.AudioVideo.SRC";
inline constexpr std::string_view kMeanTimeBetweenRebuffersAudioVideoMse =
    "Media.MeanTimeBetweenRebuffers.AudioVideo.MSE";
inline constexpr std::string_view kMeanTimeBetweenRebuffersAudioVideoEme =
    "Media.MeanTimeBetweenRebuffers.AudioVideo.EME";
inline constexpr std::string_view kMeanTimeBetweenRebuffersAudioVideoHls =
    "Media.MeanTimeBetweenRebuffers.AudioVideo.HLS";

// Whether there were any rebuffers within a given watch time report.
inline constexpr std::string_view kRebuffersCountAudioSrc =
    "Media.RebuffersCount.Audio.SRC";
inline constexpr std::string_view kRebuffersCountAudioMse =
    "Media.RebuffersCount.Audio.MSE";
inline constexpr std::string_view kRebuffersCountAudioEme =
    "Media.RebuffersCount.Audio.EME";
inline constexpr std::string_view kRebuffersCountAudioHls =
    "Media.RebuffersCount.Audio.HLS";
inline constexpr std::string_view kRebuffersCountAudioVideoSrc =
    "Media.RebuffersCount.AudioVideo.SRC";
inline constexpr std::string_view kRebuffersCountAudioVideoMse =
    "Media.RebuffersCount.AudioVideo.MSE";
inline constexpr std::string_view kRebuffersCountAudioVideoEme =
    "Media.RebuffersCount.AudioVideo.EME";
inline constexpr std::string_view kRebuffersCountAudioVideoHls =
    "Media.RebuffersCount.AudioVideo.HLS";

// Amount of watch time less than minimum required, which ends up not being
// reported as part of the standard WatchTime keys. Allows estimation of an
// upper bound on uncollected watch time.
inline constexpr std::string_view kDiscardedWatchTimeAudioSrc =
    "Media.WatchTime.Audio.Discarded.SRC";
inline constexpr std::string_view kDiscardedWatchTimeAudioMse =
    "Media.WatchTime.Audio.Discarded.MSE";
inline constexpr std::string_view kDiscardedWatchTimeAudioEme =
    "Media.WatchTime.Audio.Discarded.EME";
inline constexpr std::string_view kDiscardedWatchTimeAudioHls =
    "Media.WatchTime.Audio.Discarded.HLS";
inline constexpr std::string_view kDiscardedWatchTimeAudioVideoSrc =
    "Media.WatchTime.AudioVideo.Discarded.SRC";
inline constexpr std::string_view kDiscardedWatchTimeAudioVideoMse =
    "Media.WatchTime.AudioVideo.Discarded.MSE";
inline constexpr std::string_view kDiscardedWatchTimeAudioVideoEme =
    "Media.WatchTime.AudioVideo.Discarded.EME";
inline constexpr std::string_view kDiscardedWatchTimeAudioVideoHls =
    "Media.WatchTime.AudioVideo.Discarded.HLS";

// HDR watch time metrics.
inline constexpr std::string_view kWatchTimeAudioVideoHdrAll =
    "Media.WatchTime.AudioVideo.HDR.All";
inline constexpr std::string_view kWatchTimeAudioVideoHdrEme =
    "Media.WatchTime.AudioVideo.HDR.EME";
inline constexpr std::string_view kWatchTimeAudioVideoMediaFoundationHdrAll =
    "Media.WatchTime.AudioVideo.MediaFoundation.HDR.All";
inline constexpr std::string_view kWatchTimeAudioVideoMediaFoundationHdrEme =
    "Media.WatchTime.AudioVideo.MediaFoundation.HDR.EME";

// Returns the UMA key name associated with a given WatchTimeKey or an empty
// string if they key should not be logged to UMA.
MEDIA_EXPORT std::string_view ConvertWatchTimeKeyToStringForUma(
    WatchTimeKey key);

}  // namespace media

#endif  // MEDIA_BASE_WATCH_TIME_KEYS_H_
