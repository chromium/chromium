// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUDIO_PUMP_H_
#define REMOTING_PROTOCOL_AUDIO_PUMP_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/audio_stream.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class AudioEncoder;
class AudioPacket;

namespace protocol {

class AudioStub;
class AudioSource;

// AudioPump is responsible for fetching audio data from the AudioCapturer and
// encoding it before passing it to the AudioStub for delivery to the client.
// Audio data will be downmixed to stereo if needed. Audio is captured and
// encoded on the audio thread and then passed to AudioStub on the network
// thread.
class AudioPump : public AudioStream {
 public:
  // The caller must ensure that the |audio_stub| is not destroyed until the
  // pump is destroyed.
  AudioPump(scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
            std::unique_ptr<AudioSource> audio_source,
            std::unique_ptr<AudioEncoder> audio_encoder,
            AudioStub* audio_stub);

  AudioPump(const AudioPump&) = delete;
  AudioPump& operator=(const AudioPump&) = delete;

  ~AudioPump() override;

  // AudioStream interface.
  void Pause(bool pause) override;

 private:
  class Core;

  // Called on the network thread to send a captured packet to the audio stub.
  void SendAudioPacket(std::unique_ptr<AudioPacket> packet, int size);

  // Callback for BufferedSocketWriter.
  void OnPacketSent(int size);

  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;
  raw_ptr<AudioStub> audio_stub_;

  std::unique_ptr<Core> core_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<AudioPump> weak_factory_{this};
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_AUDIO_PUMP_H_
