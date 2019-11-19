// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_SOURCE_PARAMETERS_H_
#define MEDIA_AUDIO_AUDIO_SOURCE_PARAMETERS_H_

#include <string>

#include "base/optional.h"
#include "base/unguessable_token.h"
#include "media/base/audio_processing.h"
#include "media/base/media_export.h"

namespace media {

// The set of parameters used to create an AudioInputDevice.
// If |session_id| is nonzero, it is used by the browser
// to select the correct input device ID. If |session_id| is zero, the default
// input device will be selected. This is the state when default constructed.
struct MEDIA_EXPORT AudioSourceParameters final {
  AudioSourceParameters();
  explicit AudioSourceParameters(const base::UnguessableToken& session_id);
  AudioSourceParameters(const AudioSourceParameters& params);
  ~AudioSourceParameters();

  base::UnguessableToken session_id;

  struct MEDIA_EXPORT ProcessingConfig {
    ProcessingConfig(base::UnguessableToken id,
                     AudioProcessingSettings settings);
    base::UnguessableToken id;
    AudioProcessingSettings settings;
  };

  base::Optional<ProcessingConfig> processing;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_SOURCE_PARAMETERS_H_
