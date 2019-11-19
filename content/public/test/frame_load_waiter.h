// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FRAME_LOAD_WAITER_H_
#define CONTENT_PUBLIC_TEST_FRAME_LOAD_WAITER_H_

#include "base/macros.h"
#include "base/run_loop.h"
#include "content/public/renderer/render_frame_observer.h"

namespace content {

// Helper class to spin the run loop when waiting for a frame load to complete.
// Blink uses a threaded HTML parser, so it's no longer sufficient to just spin
// the run loop once to process all pending messages.
class FrameLoadWaiter : public RenderFrameObserver {
 public:
  explicit FrameLoadWaiter(RenderFrame* frame);

  // Note: single-process browser tests need to enable nestable tasks by
  // instantiating a base::MessageLoopCurrent::ScopedNestableTaskAllower or this
  // method will never return.
  void Wait();

 private:
  // RenderFrameObserver implementation.
  void DidFinishLoad() override;
  void OnDestruct() override;

  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  bool did_load_ = false;

  DISALLOW_COPY_AND_ASSIGN(FrameLoadWaiter);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FRAME_LOAD_WAITER_H_
