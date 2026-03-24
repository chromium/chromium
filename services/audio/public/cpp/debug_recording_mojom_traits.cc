// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/debug_recording_mojom_traits.h"

#include "base/notreached.h"

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
    case media::AudioDebugRecordingStreamType::kLoopback:
      return audio::mojom::DebugRecordingStreamType::kLoopback;
  }
  NOTREACHED();
}

media::AudioDebugRecordingStreamType
EnumTraits<audio::mojom::DebugRecordingStreamType,
           media::AudioDebugRecordingStreamType>::
    FromMojom(audio::mojom::DebugRecordingStreamType stream_type) {
  switch (stream_type) {
    case audio::mojom::DebugRecordingStreamType::kInput:
      return media::AudioDebugRecordingStreamType::kInput;
    case audio::mojom::DebugRecordingStreamType::kOutput:
      return media::AudioDebugRecordingStreamType::kOutput;
    case audio::mojom::DebugRecordingStreamType::kLoopback:
      return media::AudioDebugRecordingStreamType::kLoopback;
  }
  NOTREACHED();
}

}  // namespace mojo
