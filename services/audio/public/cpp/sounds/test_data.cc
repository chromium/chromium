// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/sounds/test_data.h"

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

namespace audio {

TestObserver::TestObserver(const base::RepeatingClosure& quit)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      quit_(quit),
      num_play_requests_(0),
      num_stop_requests_(0),
      cursor_(0) {}

TestObserver::~TestObserver() = default;

void TestObserver::Initialize(
    media::AudioRendererSink::RenderCallback* callback,
    media::AudioParameters params) {
  callback_ = callback;
  bus_ = media::AudioBus::Create(params);
}

void TestObserver::OnPlay() {
  ++num_play_requests_;
  is_playing = true;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TestObserver::Render, base::Unretained(this)));
}

void TestObserver::Render() {
  if (!is_playing)
    return;
  if (callback_->Render(base::TimeDelta::FromSeconds(0), base::TimeTicks::Now(),
                        0, bus_.get())) {
    task_runner_->PostTask(FROM_HERE, base::BindOnce(&TestObserver::Render,
                                                     base::Unretained(this)));
  }
}

void TestObserver::OnStop(size_t cursor) {
  ++num_stop_requests_;
  cursor_ = cursor;
  is_playing = false;
  task_runner_->PostTask(FROM_HERE, quit_);
}

}  // namespace audio
