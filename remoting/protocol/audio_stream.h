// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUDIO_STREAM_H_
#define REMOTING_PROTOCOL_AUDIO_STREAM_H_

namespace remoting::protocol {

// AudioStream is responsible for fetching audio data from an AudioSource, and
// sending it to the client.
class AudioStream {
 public:
  AudioStream() = default;
  virtual ~AudioStream() = default;

  // Pauses or resumes audio on a running session. This leaves the audio
  // capturer running, and only affects whether or not the captured audio is
  // encoded and sent on the wire.
  virtual void Pause(bool pause) = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_AUDIO_STREAM_H_
