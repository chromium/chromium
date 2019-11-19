// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_DELETE_INTENTION_H_
#define CONTENT_COMMON_FRAME_DELETE_INTENTION_H_

namespace content {

enum class FrameDeleteIntention {
  // The frame being deleted isn't a (speculative) main frame.
  kNotMainFrame,
  // The frame being deleted is a speculative main frame, and it is being
  // deleted as part of the shutdown for that WebContents. The entire RenderView
  // etc will be destroyed by a separate IPC sent later.
  kSpeculativeMainFrameForShutdown,
  // The frame being deleted is a speculative main frame, and it is being
  // deleted because the speculative navigation was cancelled. This is not part
  // of shutdown.
  kSpeculativeMainFrameForNavigationCancelled,

  kMaxValue = kSpeculativeMainFrameForNavigationCancelled
};

}  // namespace content

#endif  // CONTENT_COMMON_FRAME_DELETE_INTENTION_H_
