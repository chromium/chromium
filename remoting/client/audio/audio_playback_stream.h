// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_AUDIO_AUDIO_PLAYBACK_STREAM_H_
#define REMOTING_CLIENT_AUDIO_AUDIO_PLAYBACK_STREAM_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/audio_stub.h"

namespace remoting {

class AudioPlaybackSink;

// An AudioStub implementation that buffers AudioPackets and feeds them to
// an AudioPlaybackSink.
// AudioPlaybackStream must be used and destroyed on the same thread after it
// is created, while it will use and destroy |audio_sink| on the thread of
// |audio_task_runner|.
class AudioPlaybackStream : public protocol::AudioStub {
 public:
  // |audio_sink|: The AudioPlaybackSink that receives audio data.
  // |audio_task_runner|: The task runner where |audio_sink| will be run.
  AudioPlaybackStream(
      std::unique_ptr<AudioPlaybackSink> audio_sink,
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner);

  AudioPlaybackStream(const AudioPlaybackStream&) = delete;
  AudioPlaybackStream& operator=(const AudioPlaybackStream&) = delete;

  ~AudioPlaybackStream() override;

  // AudioStub implementations.
  void ProcessAudioPacket(std::unique_ptr<AudioPacket> packet,
                          base::OnceClosure done) override;

 private:
  class Core;

  THREAD_CHECKER(thread_checker_);

  std::unique_ptr<Core> core_;

  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_AUDIO_AUDIO_PLAYBACK_STREAM_H_
