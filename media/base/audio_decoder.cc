// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_decoder.h"

#include "media/base/audio_buffer.h"

namespace media {

AudioDecoder::AudioDecoder() = default;

AudioDecoder::~AudioDecoder() = default;

bool AudioDecoder::NeedsBitstreamConversion() const {
  return false;
}

}  // namespace media
