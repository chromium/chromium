// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_CLIENT_H_
#define MEDIA_BASE_MEDIA_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_parameters.h"
#include "media/base/key_system_properties.h"
#include "media/base/media_export.h"
#include "media/base/media_types.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "ui/gfx/color_space.h"
#include "url/gurl.h"

namespace media {

class MediaClient;

// Setter for the client. If a customized client is needed, it should be set
// early, before the client could possibly be used.
MEDIA_EXPORT void SetMediaClient(MediaClient* media_client);

// Media's embedder API should only be used by media.
#if defined(IS_MEDIA_IMPL) || defined(MEDIA_BLINK_IMPLEMENTATION)
// Getter for the client. Returns NULL if no customized client is needed.
MEDIA_EXPORT MediaClient* GetMediaClient();
#endif

// A client interface for embedders (e.g. content/renderer) to provide
// customized key systems and decoders.
class MEDIA_EXPORT MediaClient {
 public:
  MediaClient();
  virtual ~MediaClient();

  // Adds properties for supported key systems.
  virtual void AddSupportedKeySystems(
      std::vector<std::unique_ptr<KeySystemProperties>>* key_systems) = 0;

  // Returns whether client key systems properties should be updated.
  // TODO(chcunningham): Refactor this to a proper change "observer" API that is
  // less fragile (don't assume AddSupportedKeySystems has just one caller).
  virtual bool IsKeySystemsUpdateNeeded() = 0;

  // Returns true if the given audio config is supported.
  virtual bool IsSupportedAudioType(const AudioType& type) = 0;

  // Returns true if the given video config is supported.
  virtual bool IsSupportedVideoType(const VideoType& type) = 0;

  // Returns true if the compressed audio |codec| format is supported by the
  // audio sink.
  virtual bool IsSupportedBitstreamAudioCodec(AudioCodec codec) = 0;

  // Optionally returns audio renderer algorithm parameters.
  virtual base::Optional<::media::AudioRendererAlgorithmParameters>
  GetAudioRendererAlgorithmParameters(AudioParameters audio_parameters) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_CLIENT_H_
