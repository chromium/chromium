// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/audio/audio_playback_stream.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/client/audio/audio_jitter_buffer.h"
#include "remoting/client/audio/audio_playback_sink.h"

namespace remoting {

class AudioPlaybackStream::Core {
 public:
  explicit Core(std::unique_ptr<AudioPlaybackSink> audio_sink);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core();

  void AddAudioPacket(std::unique_ptr<AudioPacket> packet);

 private:
  void ResetStreamFormat(const AudioStreamFormat& format);

  // |jitter_buffer_| must outlive |audio_sink_|.
  std::unique_ptr<AudioJitterBuffer> jitter_buffer_;
  std::unique_ptr<AudioPlaybackSink> audio_sink_;
};

AudioPlaybackStream::Core::Core(std::unique_ptr<AudioPlaybackSink> audio_sink) {
  jitter_buffer_ = std::make_unique<AudioJitterBuffer>(base::BindRepeating(
      &AudioPlaybackStream::Core::ResetStreamFormat, base::Unretained(this)));
  audio_sink_ = std::move(audio_sink);
  audio_sink_->SetDataSupplier(jitter_buffer_.get());
}

AudioPlaybackStream::Core::~Core() = default;

void AudioPlaybackStream::Core::AddAudioPacket(
    std::unique_ptr<AudioPacket> packet) {
  jitter_buffer_->AddAudioPacket(std::move(packet));
}

void AudioPlaybackStream::Core::ResetStreamFormat(
    const AudioStreamFormat& format) {
  audio_sink_->ResetStreamFormat(format);
}

// AudioPlaybackStream implementations.

AudioPlaybackStream::AudioPlaybackStream(
    std::unique_ptr<AudioPlaybackSink> audio_sink,
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner)
    : audio_task_runner_(audio_task_runner) {
  DETACH_FROM_THREAD(thread_checker_);

  core_ = std::make_unique<Core>(std::move(audio_sink));
}

AudioPlaybackStream::~AudioPlaybackStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void AudioPlaybackStream::ProcessAudioPacket(
    std::unique_ptr<AudioPacket> packet,
    base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&Core::AddAudioPacket, base::Unretained(core_.get()),
                     std::move(packet)),
      std::move(done));
}

}  // namespace remoting
