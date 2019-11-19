// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_SINK_PARAMETERS_H_
#define MEDIA_AUDIO_AUDIO_SINK_PARAMETERS_H_

#include <string>

#include "base/optional.h"
#include "base/unguessable_token.h"
#include "media/base/media_export.h"

namespace media {

// The set of parameters used to create an AudioOutputDevice.
// |session_id| and |device_id| are used to select which device to
// use. |device_id| is preferred over |session_id| if both are set
// (i.e. session_id is nonzero).  If neither is set, the default output device
// will be selected. This is the state when default constructed.
// If the optional |processing_id| is provided, it is used to indicate that this
// stream is to be used as the reference signal during audio processing. An
// audio source must be constructed with the same processing id to complete the
// association.
struct MEDIA_EXPORT AudioSinkParameters final {
  AudioSinkParameters();
  AudioSinkParameters(const base::UnguessableToken& session_id,
                      const std::string& device_id);
  AudioSinkParameters(const AudioSinkParameters& params);
  ~AudioSinkParameters();

  base::UnguessableToken session_id;
  std::string device_id;
  base::Optional<base::UnguessableToken> processing_id;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_SINK_PARAMETERS_H_
