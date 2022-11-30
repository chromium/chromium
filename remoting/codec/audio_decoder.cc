// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/audio_decoder.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "remoting/codec/audio_decoder_opus.h"
#include "remoting/protocol/session_config.h"

namespace remoting {

std::unique_ptr<AudioDecoder> AudioDecoder::CreateAudioDecoder(
    const protocol::SessionConfig& config) {
  if (config.audio_config().codec == protocol::ChannelConfig::CODEC_OPUS) {
    return base::WrapUnique(new AudioDecoderOpus());
  }

  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace remoting
