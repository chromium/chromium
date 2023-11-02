// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_AUDIO_AUDIO_STREAM_FORMAT_H_
#define REMOTING_CLIENT_AUDIO_AUDIO_STREAM_FORMAT_H_

namespace remoting {

struct AudioStreamFormat {
  bool operator==(const AudioStreamFormat& other) const;
  bool operator!=(const AudioStreamFormat& other) const;

  int bytes_per_sample = 0;
  int channels = 0;
  int sample_rate = 0;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_AUDIO_AUDIO_STREAM_FORMAT_H_
