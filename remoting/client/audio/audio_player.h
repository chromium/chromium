// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_AUDIO_AUDIO_PLAYER_H_
#define REMOTING_CLIENT_AUDIO_AUDIO_PLAYER_H_

#include <cstdint>
#include <list>
#include <memory>

#include "base/synchronization/lock.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/audio_stub.h"

namespace remoting {

// TODO(nicholss): Update legacy audio player to use new audio buffer code.
class AudioPlayer : public protocol::AudioStub {
 public:
  // The number of channels in the audio stream (only supporting stereo audio
  // for now).
  static const int kChannels = 2;
  static const int kSampleSizeBytes = 2;

  AudioPlayer(const AudioPlayer&) = delete;
  AudioPlayer& operator=(const AudioPlayer&) = delete;

  ~AudioPlayer() override;

  // protocol::AudioStub implementation.
  void ProcessAudioPacket(std::unique_ptr<AudioPacket> packet,
                          base::OnceClosure done) override;

 protected:
  AudioPlayer();

  // Return the recommended number of samples to include in a frame.
  virtual uint32_t GetSamplesPerFrame() = 0;

  // Resets the audio player and starts playback.
  // Returns true on success.
  virtual bool ResetAudioPlayer(AudioPacket::SamplingRate sampling_rate) = 0;

  // Function called by the browser when it needs more audio samples.
  static void AudioPlayerCallback(void* samples,
                                  uint32_t buffer_size,
                                  void* data);

  // Function called by the subclass when it needs more audio samples to fill
  // its buffer. Will fill the buffer with 0's if no sample is available.
  void FillWithSamples(void* samples, uint32_t buffer_size);

 private:
  friend class AudioPlayerTest;

  typedef std::list<std::unique_ptr<AudioPacket>> AudioPacketQueue;

  void ResetQueue();

  AudioPacket::SamplingRate sampling_rate_;

  bool start_failed_;

  // Protects |queued_packets_|, |queued_samples_ and |bytes_consumed_|. This is
  // necessary to prevent races, because Pepper will call the  callback on a
  // separate thread.
  base::Lock lock_;

  AudioPacketQueue queued_packets_;
  int queued_bytes_;

  // The number of bytes from |queued_packets_| that have been consumed.
  size_t bytes_consumed_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_AUDIO_AUDIO_PLAYER_H_
