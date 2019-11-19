// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_overscroll_refresh_handler_android.h"

#include "content/public/test/test_utils.h"

namespace content {

MockOverscrollRefreshHandlerAndroid::MockOverscrollRefreshHandlerAndroid()
    : ui::OverscrollRefreshHandler(nullptr) {}

MockOverscrollRefreshHandlerAndroid::~MockOverscrollRefreshHandlerAndroid() {}

bool MockOverscrollRefreshHandlerAndroid::PullStart(OverscrollAction type,
                                                    float startx,
                                                    float starty,
                                                    bool navigateForward) {
  // The first GestureScrollUpdate starts the pull, but does not update the
  // pull. For the purpose of testing, we'll be consistent with aura
  // overscroll and consider this an update.
  OnPullUpdate();
  return true;
}

void MockOverscrollRefreshHandlerAndroid::PullUpdate(float, float) {
  OnPullUpdate();
}

void MockOverscrollRefreshHandlerAndroid::PullRelease(bool) {
  OnPullEnd();
}

void MockOverscrollRefreshHandlerAndroid::PullReset() {
  OnPullEnd();
}

void MockOverscrollRefreshHandlerAndroid::WaitForUpdate() {
  if (!seen_update_)
    update_message_loop_runner_->Run();
}

void MockOverscrollRefreshHandlerAndroid::WaitForEnd() {
  if (!pull_ended_)
    end_message_loop_runner_->Run();
}

void MockOverscrollRefreshHandlerAndroid::Reset() {
  update_message_loop_runner_ = new MessageLoopRunner;
  end_message_loop_runner_ = new MessageLoopRunner;
  seen_update_ = false;
  pull_ended_ = false;
}

void MockOverscrollRefreshHandlerAndroid::OnPullUpdate() {
  seen_update_ = true;
  if (update_message_loop_runner_->loop_running())
    update_message_loop_runner_->Quit();
}

void MockOverscrollRefreshHandlerAndroid::OnPullEnd() {
  pull_ended_ = true;
  if (end_message_loop_runner_->loop_running())
    end_message_loop_runner_->Quit();
}

}  // namespace content
