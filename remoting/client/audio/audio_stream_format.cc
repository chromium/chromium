// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/audio/audio_stream_format.h"

namespace remoting {

bool AudioStreamFormat::operator==(const AudioStreamFormat& other) const {
  return bytes_per_sample == other.bytes_per_sample &&
         channels == other.channels && sample_rate == other.sample_rate;
}

bool AudioStreamFormat::operator!=(const AudioStreamFormat& other) const {
  return !(*this == other);
}

}  // namespace remoting
