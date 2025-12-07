// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/watch_time_keys.h"

#include "base/notreached.h"

namespace {

// Note: While we could switch these keys to be dynamically constructed, testing
// shows this doesn't save any binary size and just complicates the code.

// Audio+video watch time metrics.
constexpr char kWatchTimeAudioVideoAll[] = "Media.WatchTime.AudioVideo.All";
constexpr char kWatchTimeAudioVideoMse[] = "Media.WatchTime.AudioVideo.MSE";
constexpr char kWatchTimeAudioVideoEme[] = "Media.WatchTime.AudioVideo.EME";
constexpr char kWatchTimeAudioVideoSrc[] = "Media.WatchTime.AudioVideo.SRC";
constexpr char kWatchTimeAudioVideoHls[] = "Media.WatchTime.AudioVideo.HLS";
constexpr char kWatchTimeAudioVideoBattery[] =
    "Media.WatchTime.AudioVideo.Battery";
constexpr char kWatchTimeAudioVideoAc[] = "Media.WatchTime.AudioVideo.AC";
constexpr char kWatchTimeAudioVideoDisplayFullscreen[] =
    "Media.WatchTime.AudioVideo.DisplayFullscreen";
constexpr char kWatchTimeAudioVideoDisplayInline[] =
    "Media.WatchTime.AudioVideo.DisplayInline";
constexpr char kWatchTimeAudioVideoDisplayPictureInPicture[] =
    "Media.WatchTime.AudioVideo.DisplayPictureInPicture";
constexpr char kWatchTimeAudioVideoDominantVisibleContent[] =
    "Media.WatchTime.AudioVideo.DominantVisibleContent";
constexpr char kWatchTimeAudioVideoAuxiliaryVisibleContent[] =
    "Media.WatchTime.AudioVideo.AuxiliaryVisibleContent";
constexpr char kWatchTimeAudioVideoEmbeddedExperience[] =
    "Media.WatchTime.AudioVideo.EmbeddedExperience";
constexpr char kWatchTimeAudioVideoNativeControlsOn[] =
    "Media.WatchTime.AudioVideo.NativeControlsOn";
constexpr char kWatchTimeAudioVideoNativeControlsOff[] =
    "Media.WatchTime.AudioVideo.NativeControlsOff";

// Audio only "watch time" metrics.
constexpr char kWatchTimeAudioAll[] = "Media.WatchTime.Audio.All";
constexpr char kWatchTimeAudioMse[] = "Media.WatchTime.Audio.MSE";
constexpr char kWatchTimeAudioEme[] = "Media.WatchTime.Audio.EME";
constexpr char kWatchTimeAudioSrc[] = "Media.WatchTime.Audio.SRC";
constexpr char kWatchTimeAudioHls[] = "Media.WatchTime.Audio.HLS";
constexpr char kWatchTimeAudioBattery[] = "Media.WatchTime.Audio.Battery";
constexpr char kWatchTimeAudioAc[] = "Media.WatchTime.Audio.AC";
constexpr char kWatchTimeAudioEmbeddedExperience[] =
    "Media.WatchTime.Audio.EmbeddedExperience";
constexpr char kWatchTimeAudioNativeControlsOn[] =
    "Media.WatchTime.Audio.NativeControlsOn";
constexpr char kWatchTimeAudioNativeControlsOff[] =
    "Media.WatchTime.Audio.NativeControlsOff";

constexpr char kWatchTimeAudioBackgroundAll[] =
    "Media.WatchTime.Audio.Background.All";
constexpr char kWatchTimeAudioBackgroundMse[] =
    "Media.WatchTime.Audio.Background.MSE";
constexpr char kWatchTimeAudioBackgroundEme[] =
    "Media.WatchTime.Audio.Background.EME";
constexpr char kWatchTimeAudioBackgroundSrc[] =
    "Media.WatchTime.Audio.Background.SRC";
constexpr char kWatchTimeAudioBackgroundHls[] =
    "Media.WatchTime.Audio.Background.HLS";
constexpr char kWatchTimeAudioBackgroundBattery[] =
    "Media.WatchTime.Audio.Background.Battery";
constexpr char kWatchTimeAudioBackgroundAc[] =
    "Media.WatchTime.Audio.Background.AC";
constexpr char kWatchTimeAudioBackgroundEmbeddedExperience[] =
    "Media.WatchTime.Audio.Background.EmbeddedExperience";

// Audio+video background watch time metrics.
constexpr char kWatchTimeAudioVideoBackgroundAll[] =
    "Media.WatchTime.AudioVideo.Background.All";
constexpr char kWatchTimeAudioVideoBackgroundMse[] =
    "Media.WatchTime.AudioVideo.Background.MSE";
constexpr char kWatchTimeAudioVideoBackgroundEme[] =
    "Media.WatchTime.AudioVideo.Background.EME";
constexpr char kWatchTimeAudioVideoBackgroundSrc[] =
    "Media.WatchTime.AudioVideo.Background.SRC";
constexpr char kWatchTimeAudioVideoBackgroundHls[] =
    "Media.WatchTime.AudioVideo.Background.HLS";
constexpr char kWatchTimeAudioVideoBackgroundBattery[] =
    "Media.WatchTime.AudioVideo.Background.Battery";
constexpr char kWatchTimeAudioVideoBackgroundAc[] =
    "Media.WatchTime.AudioVideo.Background.AC";
constexpr char kWatchTimeAudioVideoBackgroundEmbeddedExperience[] =
    "Media.WatchTime.AudioVideo.Background.EmbeddedExperience";

// Audio+video muted watch time metrics.
constexpr char kWatchTimeAudioVideoMutedAll[] =
    "Media.WatchTime.AudioVideo.Muted.All";
constexpr char kWatchTimeAudioVideoMutedMse[] =
    "Media.WatchTime.AudioVideo.Muted.MSE";
constexpr char kWatchTimeAudioVideoMutedEme[] =
    "Media.WatchTime.AudioVideo.Muted.EME";
constexpr char kWatchTimeAudioVideoMutedSrc[] =
    "Media.WatchTime.AudioVideo.Muted.SRC";
constexpr char kWatchTimeAudioVideoMutedHls[] =
    "Media.WatchTime.AudioVideo.Muted.HLS";
constexpr char kWatchTimeAudioVideoMutedDominantVisibleContent[] =
    "Media.WatchTime.AudioVideo.Muted.DominantVisibleContent";
constexpr char kWatchTimeAudioVideoMutedAuxiliaryVisibleContent[] =
    "Media.WatchTime.AudioVideo.Muted.AuxiliaryVisibleContent";

// Media Foundation AudioVideo watch time metric.
constexpr char kWatchTimeAudioVideoMediaFoundationAll[] =
    "Media.WatchTime.AudioVideo.MediaFoundation.All";
constexpr char kWatchTimeAudioVideoMediaFoundationEme[] =
    "Media.WatchTime.AudioVideo.MediaFoundation.Eme";

// Automatic picture in picture for media playback watch time metrics.
constexpr char kWatchTimeAudioVideoAutoPipMediaPlayback[] =
    "Media.WatchTime.AudioVideo.AutoPipMediaPlayback";
constexpr char kWatchTimeAudioAutoPipMediaPlayback[] =
    "Media.WatchTime.Audio.AutoPipMediaPlayback";

}  // namespace

namespace media {

std::string_view ConvertWatchTimeKeyToStringForUma(WatchTimeKey key) {
  // WARNING: Returning a non-empty value will log the key to UMA.
  switch (key) {
    case WatchTimeKey::kAudioAll:
      return kWatchTimeAudioAll;
    case WatchTimeKey::kAudioAutoPipMediaPlayback:
      return kWatchTimeAudioAutoPipMediaPlayback;
    case WatchTimeKey::kAudioMse:
      return kWatchTimeAudioMse;
    case WatchTimeKey::kAudioEme:
      return kWatchTimeAudioEme;
    case WatchTimeKey::kAudioSrc:
      return kWatchTimeAudioSrc;
    case WatchTimeKey::kAudioHls:
      return kWatchTimeAudioHls;
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
    case WatchTimeKey::kAudioBackgroundHls:
      return kWatchTimeAudioBackgroundHls;
    case WatchTimeKey::kAudioBackgroundBattery:
      return kWatchTimeAudioBackgroundBattery;
    case WatchTimeKey::kAudioBackgroundAc:
      return kWatchTimeAudioBackgroundAc;
    case WatchTimeKey::kAudioBackgroundEmbeddedExperience:
      return kWatchTimeAudioBackgroundEmbeddedExperience;
    case WatchTimeKey::kAudioVideoAll:
      return kWatchTimeAudioVideoAll;
    case WatchTimeKey::kAudioVideoAutoPipMediaPlayback:
      return kWatchTimeAudioVideoAutoPipMediaPlayback;
    case WatchTimeKey::kAudioVideoMse:
      return kWatchTimeAudioVideoMse;
    case WatchTimeKey::kAudioVideoEme:
      return kWatchTimeAudioVideoEme;
    case WatchTimeKey::kAudioVideoSrc:
      return kWatchTimeAudioVideoSrc;
    case WatchTimeKey::kAudioVideoHls:
      return kWatchTimeAudioVideoHls;
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
    case WatchTimeKey::kAudioVideoDominantVisibleContent:
      return kWatchTimeAudioVideoDominantVisibleContent;
    case WatchTimeKey::kAudioVideoAuxiliaryVisibleContent:
      return kWatchTimeAudioVideoAuxiliaryVisibleContent;
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
    case WatchTimeKey::kAudioVideoBackgroundHls:
      return kWatchTimeAudioVideoBackgroundHls;
    case WatchTimeKey::kAudioVideoBackgroundBattery:
      return kWatchTimeAudioVideoBackgroundBattery;
    case WatchTimeKey::kAudioVideoBackgroundAc:
      return kWatchTimeAudioVideoBackgroundAc;
    case WatchTimeKey::kAudioVideoBackgroundEmbeddedExperience:
      return kWatchTimeAudioVideoBackgroundEmbeddedExperience;
    case WatchTimeKey::kAudioVideoHdrAll:
      return kWatchTimeAudioVideoHdrAll;
    case WatchTimeKey::kAudioVideoHdrEme:
      return kWatchTimeAudioVideoHdrEme;
    case WatchTimeKey::kAudioVideoMutedAll:
      return kWatchTimeAudioVideoMutedAll;
    case WatchTimeKey::kAudioVideoMutedMse:
      return kWatchTimeAudioVideoMutedMse;
    case WatchTimeKey::kAudioVideoMutedEme:
      return kWatchTimeAudioVideoMutedEme;
    case WatchTimeKey::kAudioVideoMutedSrc:
      return kWatchTimeAudioVideoMutedSrc;
    case WatchTimeKey::kAudioVideoMutedHls:
      return kWatchTimeAudioVideoMutedHls;
    case WatchTimeKey::kAudioVideoMutedDominantVisibleContent:
      return kWatchTimeAudioVideoMutedDominantVisibleContent;
    case WatchTimeKey::kAudioVideoMutedAuxiliaryVisibleContent:
      return kWatchTimeAudioVideoMutedAuxiliaryVisibleContent;
    case WatchTimeKey::kAudioVideoMediaFoundationAll:
      return kWatchTimeAudioVideoMediaFoundationAll;
    case WatchTimeKey::kAudioVideoMediaFoundationHdrAll:
      return kWatchTimeAudioVideoMediaFoundationHdrAll;
    case WatchTimeKey::kAudioVideoMediaFoundationHdrEme:
      return kWatchTimeAudioVideoMediaFoundationHdrEme;
    case WatchTimeKey::kAudioVideoMediaFoundationEme:
      return kWatchTimeAudioVideoMediaFoundationEme;
    // WARNING: Returning a non-empty value will log the key to UMA.

    // The following keys are not reported to UMA and thus have no conversion.
    // We don't report keys to UMA that we don't have a strong use case for
    // since UMA requires us to break out each state manually (ac, inline, etc).
    case WatchTimeKey::kAudioDisplayFullscreen:
    case WatchTimeKey::kAudioDisplayInline:
    case WatchTimeKey::kAudioDisplayPictureInPicture:
    case WatchTimeKey::kAudioVideoSdrAll:
    case WatchTimeKey::kAudioVideoSdrEme:
    case WatchTimeKey::kAudioVideoMediaFoundationSdrAll:
    case WatchTimeKey::kAudioVideoMediaFoundationSdrEme:
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
    case WatchTimeKey::kVideoHls:
    case WatchTimeKey::kVideoBattery:
    case WatchTimeKey::kVideoAc:
    case WatchTimeKey::kVideoDisplayFullscreen:
    case WatchTimeKey::kVideoDisplayInline:
    case WatchTimeKey::kVideoDisplayPictureInPicture:
    case WatchTimeKey::kVideoDominantVisibleContent:
    case WatchTimeKey::kVideoAuxiliaryVisibleContent:
    case WatchTimeKey::kVideoEmbeddedExperience:
    case WatchTimeKey::kVideoNativeControlsOn:
    case WatchTimeKey::kVideoNativeControlsOff:
    case WatchTimeKey::kVideoBackgroundAll:
    case WatchTimeKey::kVideoBackgroundMse:
    case WatchTimeKey::kVideoBackgroundEme:
    case WatchTimeKey::kVideoBackgroundSrc:
    case WatchTimeKey::kVideoBackgroundHls:
    case WatchTimeKey::kVideoBackgroundBattery:
    case WatchTimeKey::kVideoBackgroundAc:
    case WatchTimeKey::kVideoBackgroundEmbeddedExperience:
      return std::string_view();
  };

  NOTREACHED();
}

}  // namespace media
