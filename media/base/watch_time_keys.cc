// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/watch_time_keys.h"

#include "base/notreached.h"

namespace media {

// TODO(dalecurtis): Key strings aren't really necessary anymore, so instead
// of hard coding these, switch to generating them.

// Audio+video watch time metrics.
static const char kWatchTimeAudioVideoAll[] = "Media.WatchTime.AudioVideo.All";
static const char kWatchTimeAudioVideoMse[] = "Media.WatchTime.AudioVideo.MSE";
static const char kWatchTimeAudioVideoEme[] = "Media.WatchTime.AudioVideo.EME";
static const char kWatchTimeAudioVideoSrc[] = "Media.WatchTime.AudioVideo.SRC";
static const char kWatchTimeAudioVideoBattery[] =
    "Media.WatchTime.AudioVideo.Battery";
static const char kWatchTimeAudioVideoAc[] = "Media.WatchTime.AudioVideo.AC";
static const char kWatchTimeAudioVideoDisplayFullscreen[] =
    "Media.WatchTime.AudioVideo.DisplayFullscreen";
static const char kWatchTimeAudioVideoDisplayInline[] =
    "Media.WatchTime.AudioVideo.DisplayInline";
static const char kWatchTimeAudioVideoDisplayPictureInPicture[] =
    "Media.WatchTime.AudioVideo.DisplayPictureInPicture";
static const char kWatchTimeAudioVideoEmbeddedExperience[] =
    "Media.WatchTime.AudioVideo.EmbeddedExperience";
static const char kWatchTimeAudioVideoNativeControlsOn[] =
    "Media.WatchTime.AudioVideo.NativeControlsOn";
static const char kWatchTimeAudioVideoNativeControlsOff[] =
    "Media.WatchTime.AudioVideo.NativeControlsOff";

// Audio only "watch time" metrics.
static const char kWatchTimeAudioAll[] = "Media.WatchTime.Audio.All";
static const char kWatchTimeAudioMse[] = "Media.WatchTime.Audio.MSE";
static const char kWatchTimeAudioEme[] = "Media.WatchTime.Audio.EME";
static const char kWatchTimeAudioSrc[] = "Media.WatchTime.Audio.SRC";
static const char kWatchTimeAudioBattery[] = "Media.WatchTime.Audio.Battery";
static const char kWatchTimeAudioAc[] = "Media.WatchTime.Audio.AC";
static const char kWatchTimeAudioEmbeddedExperience[] =
    "Media.WatchTime.Audio.EmbeddedExperience";
static const char kWatchTimeAudioNativeControlsOn[] =
    "Media.WatchTime.Audio.NativeControlsOn";
static const char kWatchTimeAudioNativeControlsOff[] =
    "Media.WatchTime.Audio.NativeControlsOff";

static const char kWatchTimeAudioBackgroundAll[] =
    "Media.WatchTime.Audio.Background.All";
static const char kWatchTimeAudioBackgroundMse[] =
    "Media.WatchTime.Audio.Background.MSE";
static const char kWatchTimeAudioBackgroundEme[] =
    "Media.WatchTime.Audio.Background.EME";
static const char kWatchTimeAudioBackgroundSrc[] =
    "Media.WatchTime.Audio.Background.SRC";
static const char kWatchTimeAudioBackgroundBattery[] =
    "Media.WatchTime.Audio.Background.Battery";
static const char kWatchTimeAudioBackgroundAc[] =
    "Media.WatchTime.Audio.Background.AC";
static const char kWatchTimeAudioBackgroundEmbeddedExperience[] =
    "Media.WatchTime.Audio.Background.EmbeddedExperience";

// Audio+video background watch time metrics.
static const char kWatchTimeAudioVideoBackgroundAll[] =
    "Media.WatchTime.AudioVideo.Background.All";
static const char kWatchTimeAudioVideoBackgroundMse[] =
    "Media.WatchTime.AudioVideo.Background.MSE";
static const char kWatchTimeAudioVideoBackgroundEme[] =
    "Media.WatchTime.AudioVideo.Background.EME";
static const char kWatchTimeAudioVideoBackgroundSrc[] =
    "Media.WatchTime.AudioVideo.Background.SRC";
static const char kWatchTimeAudioVideoBackgroundBattery[] =
    "Media.WatchTime.AudioVideo.Background.Battery";
static const char kWatchTimeAudioVideoBackgroundAc[] =
    "Media.WatchTime.AudioVideo.Background.AC";
static const char kWatchTimeAudioVideoBackgroundEmbeddedExperience[] =
    "Media.WatchTime.AudioVideo.Background.EmbeddedExperience";

// Audio+video muted watch time metrics.
static const char kWatchTimeAudioVideoMutedAll[] =
    "Media.WatchTime.AudioVideo.Muted.All";
static const char kWatchTimeAudioVideoMutedMse[] =
    "Media.WatchTime.AudioVideo.Muted.MSE";
static const char kWatchTimeAudioVideoMutedEme[] =
    "Media.WatchTime.AudioVideo.Muted.EME";
static const char kWatchTimeAudioVideoMutedSrc[] =
    "Media.WatchTime.AudioVideo.Muted.SRC";

const char kWatchTimeUnderflowCount[] = "UnderflowCount";

const char kMeanTimeBetweenRebuffersAudioSrc[] =
    "Media.MeanTimeBetweenRebuffers.Audio.SRC";
const char kMeanTimeBetweenRebuffersAudioMse[] =
    "Media.MeanTimeBetweenRebuffers.Audio.MSE";
const char kMeanTimeBetweenRebuffersAudioEme[] =
    "Media.MeanTimeBetweenRebuffers.Audio.EME";
const char kMeanTimeBetweenRebuffersAudioVideoSrc[] =
    "Media.MeanTimeBetweenRebuffers.AudioVideo.SRC";
const char kMeanTimeBetweenRebuffersAudioVideoMse[] =
    "Media.MeanTimeBetweenRebuffers.AudioVideo.MSE";
const char kMeanTimeBetweenRebuffersAudioVideoEme[] =
    "Media.MeanTimeBetweenRebuffers.AudioVideo.EME";

const char kRebuffersCountAudioSrc[] = "Media.RebuffersCount.Audio.SRC";
const char kRebuffersCountAudioMse[] = "Media.RebuffersCount.Audio.MSE";
const char kRebuffersCountAudioEme[] = "Media.RebuffersCount.Audio.EME";
const char kRebuffersCountAudioVideoSrc[] =
    "Media.RebuffersCount.AudioVideo.SRC";
const char kRebuffersCountAudioVideoMse[] =
    "Media.RebuffersCount.AudioVideo.MSE";
const char kRebuffersCountAudioVideoEme[] =
    "Media.RebuffersCount.AudioVideo.EME";

const char kDiscardedWatchTimeAudioSrc[] =
    "Media.WatchTime.Audio.Discarded.SRC";
const char kDiscardedWatchTimeAudioMse[] =
    "Media.WatchTime.Audio.Discarded.MSE";
const char kDiscardedWatchTimeAudioEme[] =
    "Media.WatchTime.Audio.Discarded.EME";
const char kDiscardedWatchTimeAudioVideoSrc[] =
    "Media.WatchTime.AudioVideo.Discarded.SRC";
const char kDiscardedWatchTimeAudioVideoMse[] =
    "Media.WatchTime.AudioVideo.Discarded.MSE";
const char kDiscardedWatchTimeAudioVideoEme[] =
    "Media.WatchTime.AudioVideo.Discarded.EME";

base::StringPiece ConvertWatchTimeKeyToStringForUma(WatchTimeKey key) {
  // WARNING: Returning a non-empty value will log the key to UMA.
  switch (key) {
    case WatchTimeKey::kAudioAll:
      return kWatchTimeAudioAll;
    case WatchTimeKey::kAudioMse:
      return kWatchTimeAudioMse;
    case WatchTimeKey::kAudioEme:
      return kWatchTimeAudioEme;
    case WatchTimeKey::kAudioSrc:
      return kWatchTimeAudioSrc;
    case WatchTimeKey::kAudioBattery:
      return kWatchTimeAudioBattery;
    case WatchTimeKey::kAudioAc:
      return kWatchTimeAudioAc;
    case WatchTimeKey::kAudioEmbeddedExperience:
      return kWatchTimeAudioEmbeddedExperience;
    case WatchTimeKey::kAudioNativeControlsOn:
      return kWatchTimeAudioNativeControlsOn;
    case WatchTimeKey::kAudioNativeControlsOff:
      return kWatchTimeAudioNativeControlsOff;
    case WatchTimeKey::kAudioBackgroundAll:
      return kWatchTimeAudioBackgroundAll;
    case WatchTimeKey::kAudioBackgroundMse:
      return kWatchTimeAudioBackgroundMse;
    case WatchTimeKey::kAudioBackgroundEme:
      return kWatchTimeAudioBackgroundEme;
    case WatchTimeKey::kAudioBackgroundSrc:
      return kWatchTimeAudioBackgroundSrc;
    case WatchTimeKey::kAudioBackgroundBattery:
      return kWatchTimeAudioBackgroundBattery;
    case WatchTimeKey::kAudioBackgroundAc:
      return kWatchTimeAudioBackgroundAc;
    case WatchTimeKey::kAudioBackgroundEmbeddedExperience:
      return kWatchTimeAudioBackgroundEmbeddedExperience;
    case WatchTimeKey::kAudioVideoAll:
      return kWatchTimeAudioVideoAll;
    case WatchTimeKey::kAudioVideoMse:
      return kWatchTimeAudioVideoMse;
    case WatchTimeKey::kAudioVideoEme:
      return kWatchTimeAudioVideoEme;
    case WatchTimeKey::kAudioVideoSrc:
      return kWatchTimeAudioVideoSrc;
    case WatchTimeKey::kAudioVideoBattery:
      return kWatchTimeAudioVideoBattery;
    case WatchTimeKey::kAudioVideoAc:
      return kWatchTimeAudioVideoAc;
    case WatchTimeKey::kAudioVideoDisplayFullscreen:
      return kWatchTimeAudioVideoDisplayFullscreen;
    case WatchTimeKey::kAudioVideoDisplayInline:
      return kWatchTimeAudioVideoDisplayInline;
    case WatchTimeKey::kAudioVideoDisplayPictureInPicture:
      return kWatchTimeAudioVideoDisplayPictureInPicture;
    case WatchTimeKey::kAudioVideoEmbeddedExperience:
      return kWatchTimeAudioVideoEmbeddedExperience;
    case WatchTimeKey::kAudioVideoNativeControlsOn:
      return kWatchTimeAudioVideoNativeControlsOn;
    case WatchTimeKey::kAudioVideoNativeControlsOff:
      return kWatchTimeAudioVideoNativeControlsOff;
    case WatchTimeKey::kAudioVideoBackgroundAll:
      return kWatchTimeAudioVideoBackgroundAll;
    case WatchTimeKey::kAudioVideoBackgroundMse:
      return kWatchTimeAudioVideoBackgroundMse;
    case WatchTimeKey::kAudioVideoBackgroundEme:
      return kWatchTimeAudioVideoBackgroundEme;
    case WatchTimeKey::kAudioVideoBackgroundSrc:
      return kWatchTimeAudioVideoBackgroundSrc;
    case WatchTimeKey::kAudioVideoBackgroundBattery:
      return kWatchTimeAudioVideoBackgroundBattery;
    case WatchTimeKey::kAudioVideoBackgroundAc:
      return kWatchTimeAudioVideoBackgroundAc;
    case WatchTimeKey::kAudioVideoBackgroundEmbeddedExperience:
      return kWatchTimeAudioVideoBackgroundEmbeddedExperience;
    case WatchTimeKey::kAudioVideoMutedAll:
      return kWatchTimeAudioVideoMutedAll;
    case WatchTimeKey::kAudioVideoMutedMse:
      return kWatchTimeAudioVideoMutedMse;
    case WatchTimeKey::kAudioVideoMutedEme:
      return kWatchTimeAudioVideoMutedEme;
    case WatchTimeKey::kAudioVideoMutedSrc:
      return kWatchTimeAudioVideoMutedSrc;
    // WARNING: Returning a non-empty value will log the key to UMA.

    // The following keys are not reported to UMA and thus have no conversion.
    // We don't report keys to UMA that we don't have a strong use case for
    // since UMA requires us to break out each state manually (ac, inline, etc).
    case WatchTimeKey::kAudioVideoMutedBattery:
    case WatchTimeKey::kAudioVideoMutedAc:
    case WatchTimeKey::kAudioVideoMutedEmbeddedExperience:
    case WatchTimeKey::kAudioVideoMutedDisplayFullscreen:
    case WatchTimeKey::kAudioVideoMutedDisplayInline:
    case WatchTimeKey::kAudioVideoMutedDisplayPictureInPicture:
    case WatchTimeKey::kAudioVideoMutedNativeControlsOn:
    case WatchTimeKey::kAudioVideoMutedNativeControlsOff:
    case WatchTimeKey::kVideoAll:
    case WatchTimeKey::kVideoMse:
    case WatchTimeKey::kVideoEme:
    case WatchTimeKey::kVideoSrc:
    case WatchTimeKey::kVideoBattery:
    case WatchTimeKey::kVideoAc:
    case WatchTimeKey::kVideoDisplayFullscreen:
    case WatchTimeKey::kVideoDisplayInline:
    case WatchTimeKey::kVideoDisplayPictureInPicture:
    case WatchTimeKey::kVideoEmbeddedExperience:
    case WatchTimeKey::kVideoNativeControlsOn:
    case WatchTimeKey::kVideoNativeControlsOff:
    case WatchTimeKey::kVideoBackgroundAll:
    case WatchTimeKey::kVideoBackgroundMse:
    case WatchTimeKey::kVideoBackgroundEme:
    case WatchTimeKey::kVideoBackgroundSrc:
    case WatchTimeKey::kVideoBackgroundBattery:
    case WatchTimeKey::kVideoBackgroundAc:
    case WatchTimeKey::kVideoBackgroundEmbeddedExperience:
      return base::StringPiece();
  };

  NOTREACHED();
  return base::StringPiece();
}

}  // namespace media
