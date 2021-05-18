// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/supported_audio_codecs.h"

#include "media/media_buildflags.h"

namespace media {

const std::vector<AudioCodec> GetCdmSupportedAudioCodecs() {
  return {
    AudioCodec::kCodecOpus, AudioCodec::kCodecVorbis, AudioCodec::kCodecFLAC,
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
        AudioCodec::kCodecAAC,
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  };
}

}  // namespace media
