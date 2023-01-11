// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_CAPTURER_LINUX_H_
#define REMOTING_HOST_AUDIO_CAPTURER_LINUX_H_

#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/audio_silence_detector.h"
#include "remoting/host/linux/audio_pipe_reader.h"

namespace base {
class FilePath;
}

namespace remoting {

// Linux implementation of AudioCapturer interface which captures audio by
// reading samples from a Pulseaudio "pipe" sink.
class AudioCapturerLinux : public AudioCapturer,
                           public AudioPipeReader::StreamObserver {
 public:
  // Must be called to configure the capturer before the first capturer instance
  // is created. |task_runner| is an IO thread that is passed to AudioPipeReader
  // to read from the pipe.
  static void InitializePipeReader(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::FilePath& pipe_name);

  explicit AudioCapturerLinux(scoped_refptr<AudioPipeReader> pipe_reader);

  AudioCapturerLinux(const AudioCapturerLinux&) = delete;
  AudioCapturerLinux& operator=(const AudioCapturerLinux&) = delete;

  ~AudioCapturerLinux() override;

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

#endif  // REMOTING_HOST_AUDIO_CAPTURER_LINUX_H_
