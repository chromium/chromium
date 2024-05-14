// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/debug_recording_mojom_traits.h"

namespace mojo {

audio::mojom::DebugRecordingStreamType
EnumTraits<audio::mojom::DebugRecordingStreamType,
           media::AudioDebugRecordingStreamType>::
    ToMojom(media::AudioDebugRecordingStreamType stream_type) {
  switch (stream_type) {
    case media::AudioDebugRecordingStreamType::kInput:
      return audio::mojom::DebugRecordingStreamType::kInput;
    case media::AudioDebugRecordingStreamType::kOutput:
      return audio::mojom::DebugRecordingStreamType::kOutput;
  }
  NOTREACHED_IN_MIGRATION();
  return audio::mojom::DebugRecordingStreamType::kInput;
}

bool EnumTraits<audio::mojom::DebugRecordingStreamType,
                media::AudioDebugRecordingStreamType>::
    FromMojom(audio::mojom::DebugRecordingStreamType stream_type,
              media::AudioDebugRecordingStreamType* out) {
  switch (stream_type) {
    case audio::mojom::DebugRecordingStreamType::kInput:
      *out = media::AudioDebugRecordingStreamType::kInput;
      return true;
    case audio::mojom::DebugRecordingStreamType::kOutput:
      *out = media::AudioDebugRecordingStreamType::kOutput;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace mojo
