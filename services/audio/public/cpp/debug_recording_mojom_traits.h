// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_DEBUG_RECORDING_MOJOM_TRAITS_H_
#define SERVICES_AUDIO_PUBLIC_CPP_DEBUG_RECORDING_MOJOM_TRAITS_H_

#include "media/audio/audio_debug_recording_helper.h"
#include "services/audio/public/mojom/debug_recording.mojom.h"

namespace mojo {

template <>
struct EnumTraits<audio::mojom::DebugRecordingStreamType,
                  media::AudioDebugRecordingStreamType> {
  static audio::mojom::DebugRecordingStreamType ToMojom(
      media::AudioDebugRecordingStreamType stream_type);
  static bool FromMojom(audio::mojom::DebugRecordingStreamType type,
                        media::AudioDebugRecordingStreamType* output);
};

}  // namespace mojo

#endif  // SERVICES_AUDIO_PUBLIC_CPP_DEBUG_RECORDING_MOJOM_TRAITS_H_
