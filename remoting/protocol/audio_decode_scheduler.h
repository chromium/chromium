// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUDIO_DECODE_SCHEDULER_H_
#define REMOTING_PROTOCOL_AUDIO_DECODE_SCHEDULER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/audio_stub.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class AudioDecoder;
class AudioPacket;

namespace protocol {

class SessionConfig;
class AudioStub;

class AudioDecodeScheduler : public AudioStub {
 public:
  AudioDecodeScheduler(
      scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner,
      base::WeakPtr<AudioStub> audio_consumer);

  AudioDecodeScheduler(const AudioDecodeScheduler&) = delete;
  AudioDecodeScheduler& operator=(const AudioDecodeScheduler&) = delete;

  ~AudioDecodeScheduler() override;

  // Initializes decoder with the information from the protocol config.
  void Initialize(const protocol::SessionConfig& config);

  // AudioStub implementation.
  void ProcessAudioPacket(std::unique_ptr<AudioPacket> packet,
                          base::OnceClosure done) override;

 private:
  void ProcessDecodedPacket(base::OnceClosure done,
                            std::unique_ptr<AudioPacket> packet);

  scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner_;
  base::WeakPtr<AudioStub> audio_consumer_;

  // Decoder used on the audio thread.
  std::unique_ptr<AudioDecoder> decoder_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<AudioDecodeScheduler> weak_factory_{this};
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_AUDIO_DECODE_SCHEDULER_H_
