// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/sounds/test_data.h"

#include <atomic>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "media/base/audio_bus.h"

namespace audio {

TestObserver::TestObserver(base::RepeatingClosure quit,
                           base::RepeatingClosure render)
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      quit_(std::move(quit)),
      render_(std::move(render)) {}

TestObserver::~TestObserver() = default;

void TestObserver::Initialize(
    media::AudioRendererSink::RenderCallback* callback,
    media::AudioParameters params) {
  callback_ = callback;
  bus_ = media::AudioBus::Create(params);
}

void TestObserver::OnPlay() {
  ++num_play_requests_;
  is_playing_.store(true);
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&TestObserver::Render,
                                                   weak_factory_.GetWeakPtr()));
}

void TestObserver::Render() {
  if (!is_playing_.load()) {
    return;
  }
  render_.Run();
  int frames = callback_->Render(base::Seconds(0), base::TimeTicks::Now(), {},
                                 bus_.get());
  total_frames_rendered_ += frames;
  if (frames > 0) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&TestObserver::Render, weak_factory_.GetWeakPtr()));
  }
}

void TestObserver::OnStop() {
  ++num_stop_requests_;
  is_playing_.store(false);
  task_runner_->PostTask(FROM_HERE, quit_);
}

void TestObserver::OnPause() {
  ++num_pause_requests_;
  is_playing_.store(false);
  task_runner_->PostTask(FROM_HERE, quit_);
}

}  // namespace audio
