// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/mp4_muxer_delegate.h"

#include "media/base/audio_parameters.h"

namespace media {

Mp4MuxerDelegate::Mp4MuxerDelegate(Muxer::WriteDataCB write_callback) {}

void Mp4MuxerDelegate::AddVideoFrame(const Muxer::VideoParameters& params,
                                     base::StringPiece encoded_data,
                                     base::TimeTicks timestamp) {
  NOTIMPLEMENTED();
}

void Mp4MuxerDelegate::AddAudioFrame(
    const AudioParameters& params,
    base::StringPiece encoded_data,
    const AudioEncoder::CodecDescription& codec_description,
    base::TimeTicks timestamp) {
  NOTIMPLEMENTED();
}

void Mp4MuxerDelegate::Flush() {}

}  // namespace media.
