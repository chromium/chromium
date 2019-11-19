// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_OVERSCROLL_REFRESH_HANDLER_ANDROID_H_
#define CONTENT_TEST_MOCK_OVERSCROLL_REFRESH_HANDLER_ANDROID_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/test/mock_overscroll_observer.h"
#include "ui/android/overscroll_refresh_handler.h"

namespace content {

class MessageLoopRunner;

// Receives overscroll gesture updates from the android overscroll controller.
class MockOverscrollRefreshHandlerAndroid : public ui::OverscrollRefreshHandler,
                                            public MockOverscrollObserver {
 public:
  MockOverscrollRefreshHandlerAndroid();
  ~MockOverscrollRefreshHandlerAndroid() override;

  // ui::OverscrollRefreshHandler:
  bool PullStart(OverscrollAction type,
                 float startx,
                 float starty,
                 bool navigateForward) override;
  void PullUpdate(float, float) override;
  void PullRelease(bool) override;
  void PullReset() override;

  // MockOverscrollObserver:
  void WaitForUpdate() override;
  void WaitForEnd() override;
  void Reset() override;

 private:
  void OnPullUpdate();
  void OnPullEnd();

  scoped_refptr<MessageLoopRunner> update_message_loop_runner_;
  scoped_refptr<MessageLoopRunner> end_message_loop_runner_;
  bool seen_update_;
  bool pull_ended_;
  DISALLOW_COPY_AND_ASSIGN(MockOverscrollRefreshHandlerAndroid);
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_OVERSCROLL_REFRESH_HANDLER_ANDROID_H_
