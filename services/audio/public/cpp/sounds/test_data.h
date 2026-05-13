// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_TEST_DATA_H_
#define SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_TEST_DATA_H_

#include <stddef.h>

#include <atomic>
#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/audio_renderer_sink.h"
#include "services/audio/public/cpp/sounds/audio_stream_handler.h"

namespace audio {

const int kTestAudioKey = 1000;

const char kTestAudioData[] =
    "RIFF\x28\x00\x00\x00WAVEfmt \x10\x00\x00\x00"
    "\x01\x00\x02\x00\x80\xbb\x00\x00\x00\x77\x01\x00\x02\x00\x10\x00"
    "data\x04\x00\x00\x00\x01\x00\x01\x00";
const size_t kTestAudioDataSize = std::size(kTestAudioData) - 1;

class TestObserver : public AudioStreamHandler::TestObserver {
 public:
  explicit TestObserver(base::RepeatingClosure quit,
                        base::RepeatingClosure render = base::DoNothing());

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override;

  // AudioStreamHandler::TestObserver implementation:
  void Initialize(media::AudioRendererSink::RenderCallback* callback,
                  media::AudioParameters params) override;
  void OnPlay() override;
  void OnStop() override;
  void Render();

  int num_play_requests() const { return num_play_requests_; }
  int num_stop_requests() const { return num_stop_requests_; }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::RepeatingClosure quit_;
  base::RepeatingClosure render_;

  int num_play_requests_;
  int num_stop_requests_;
  // Whether the audio stream is playing. This can be set and read from
  // different threads.
  std::atomic<bool> is_playing_;
  raw_ptr<media::AudioRendererSink::RenderCallback> callback_;
  std::unique_ptr<media::AudioBus> bus_;
  base::WeakPtrFactory<TestObserver> weak_factory_{this};
};

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_TEST_DATA_H_
