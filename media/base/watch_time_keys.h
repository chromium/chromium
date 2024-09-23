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
  kAudioMse,
  kAudioEme,
  kAudioSrc,
  kAudioBattery,
  kAudioAc,
  kAudioEmbeddedExperience,
  kAudioNativeControlsOn,
  kAudioNativeControlsOff,
  kAudioBackgroundAll,
  kAudioBackgroundMse,
  kAudioBackgroundEme,
  kAudioBackgroundSrc,
  kAudioBackgroundBattery,
  kAudioBackgroundAc,
  kAudioBackgroundEmbeddedExperience,
  kAudioVideoAll,
  kAudioVideoMse,
  kAudioVideoEme,
  kAudioVideoSrc,
  kAudioVideoBattery,
  kAudioVideoAc,
  kAudioVideoDisplayFullscreen,
  kAudioVideoDisplayInline,
  kAudioVideoDisplayPictureInPicture,
  kAudioVideoEmbeddedExperience,
  kAudioVideoNativeControlsOn,
  kAudioVideoNativeControlsOff,
  kAudioVideoBackgroundAll,
  kAudioVideoBackgroundMse,
  kAudioVideoBackgroundEme,
  kAudioVideoBackgroundSrc,
  kAudioVideoBackgroundBattery,
  kAudioVideoBackgroundAc,
  kAudioVideoBackgroundEmbeddedExperience,
  kAudioVideoMutedAll,
  kAudioVideoMutedMse,
  kAudioVideoMutedEme,
  kAudioVideoMutedSrc,
  kAudioVideoMutedBattery,
  kAudioVideoMutedAc,
  kAudioVideoMutedEmbeddedExperience,
  kAudioVideoMutedDisplayFullscreen,
  kAudioVideoMutedDisplayInline,
  kAudioVideoMutedDisplayPictureInPicture,
  kAudioVideoMutedNativeControlsOn,
  kAudioVideoMutedNativeControlsOff,
  kAudioVideoMediaFoundationAll,
  kAudioVideoMediaFoundationEme,
  kVideoAll,
  kVideoMse,
  kVideoEme,
  kVideoSrc,
  kVideoBattery,
  kVideoAc,
  kVideoDisplayFullscreen,
  kVideoDisplayInline,
  kVideoDisplayPictureInPicture,
  kVideoEmbeddedExperience,
  kVideoNativeControlsOn,
  kVideoNativeControlsOff,
  kVideoBackgroundAll,
  kVideoBackgroundMse,
  kVideoBackgroundEme,
  kVideoBackgroundSrc,
  kVideoBackgroundBattery,
  kVideoBackgroundAc,
  kVideoBackgroundEmbeddedExperience,
  kWatchTimeKeyMax = kVideoBackgroundEmbeddedExperience
};

// Count of the number of underflow events during a media session.
MEDIA_EXPORT extern const char kWatchTimeUnderflowCount[];

// UMA keys for MTBR samples.
MEDIA_EXPORT extern const char kMeanTimeBetweenRebuffersAudioSrc[];
MEDIA_EXPORT extern const char kMeanTimeBetweenRebuffersAudioMse[];
MEDIA_EXPORT extern const char kMeanTimeBetweenRebuffersAudioEme[];
MEDIA_EXPORT extern const char kMeanTimeBetweenRebuffersAudioVideoSrc[];
MEDIA_EXPORT extern const char kMeanTimeBetweenRebuffersAudioVideoMse[];
MEDIA_EXPORT extern const char kMeanTimeBetweenRebuffersAudioVideoEme[];

// Whether there were any rebuffers within a given watch time report.
MEDIA_EXPORT extern const char kRebuffersCountAudioSrc[];
MEDIA_EXPORT extern const char kRebuffersCountAudioMse[];
MEDIA_EXPORT extern const char kRebuffersCountAudioEme[];
MEDIA_EXPORT extern const char kRebuffersCountAudioVideoSrc[];
MEDIA_EXPORT extern const char kRebuffersCountAudioVideoMse[];
MEDIA_EXPORT extern const char kRebuffersCountAudioVideoEme[];

// Amount of watch time less than minimum required, which ends up not being
// reported as part of the standard WatchTime keys. Allows estimation of an
// upper bound on uncollected watch time.
MEDIA_EXPORT extern const char kDiscardedWatchTimeAudioSrc[];
MEDIA_EXPORT extern const char kDiscardedWatchTimeAudioMse[];
MEDIA_EXPORT extern const char kDiscardedWatchTimeAudioEme[];
MEDIA_EXPORT extern const char kDiscardedWatchTimeAudioVideoSrc[];
MEDIA_EXPORT extern const char kDiscardedWatchTimeAudioVideoMse[];
MEDIA_EXPORT extern const char kDiscardedWatchTimeAudioVideoEme[];

// Returns the UMA key name associated with a given WatchTimeKey or an empty
// string if they key should not be logged to UMA.
MEDIA_EXPORT std::string_view ConvertWatchTimeKeyToStringForUma(
    WatchTimeKey key);

}  // namespace media

#endif  // MEDIA_BASE_WATCH_TIME_KEYS_H_
