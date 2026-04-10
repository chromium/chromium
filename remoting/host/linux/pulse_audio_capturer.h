// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PULSE_AUDIO_CAPTURER_H_
#define REMOTING_HOST_LINUX_PULSE_AUDIO_CAPTURER_H_

#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/audio_silence_detector.h"
#include "remoting/host/linux/audio_pipe_reader.h"

namespace base {
class FilePath;
}

namespace remoting {

// PulseAudio implementation of AudioCapturer interface which captures audio by
// reading samples from a PulseAudio "pipe" sink.
class PulseAudioCapturer : public AudioCapturer,
                           public AudioPipeReader::StreamObserver {
 public:
  // Returns true if audio capturing is supported on this platform. If this
  // returns true, then Create() must not return nullptr.
  static bool IsSupported();
  static std::unique_ptr<AudioCapturer> Create();

  // Must be called to configure the capturer before the first capturer instance
  // is created. |task_runner| is an IO thread that is passed to AudioPipeReader
  // to read from the pipe.
  static void InitializePipeReader(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::FilePath& pipe_name);

  explicit PulseAudioCapturer(scoped_refptr<AudioPipeReader> pipe_reader);

  PulseAudioCapturer(const PulseAudioCapturer&) = delete;
  PulseAudioCapturer& operator=(const PulseAudioCapturer&) = delete;

  ~PulseAudioCapturer() override;

  // AudioCapturer interface.
  bool Start(const PacketCapturedCallback& callback) override;

  // AudioPipeReader::StreamObserver interface.
  void OnDataRead(scoped_refptr<base::RefCountedString> data) override;

 private:
  scoped_refptr<AudioPipeReader> pipe_reader_;
  PacketCapturedCallback callback_;

  AudioSilenceDetector silence_detector_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PULSE_AUDIO_CAPTURER_H_
