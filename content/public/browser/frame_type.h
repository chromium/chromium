// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FRAME_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_FRAME_TYPE_H_

#include "content/common/content_export.h"

namespace content {

// Type of a frame, capturing whether it's a subframe or a main frame and, if
// it's a main frame, which flavour of main frame it is, which can be a primary
// main frame of a WebContents or a non-primary main frame created on behalf of
// a prerendering or a fenced frame.
//
// Inside //content, it can be accessed directly from FrameTreeNode. In
// //content/public API the embedder can figure out the frame type of a frame
// associated with a given navigation via
// NavigationHandle::GetNavigatingFrameType().
enum class FrameType {
  // A frame corresponding to an <iframe> (or a similar HTML element like
  // <object>).
  // Subframes always have a non-null parent document (FrameTreeNode::parent_ or
  // NavigationHandle::GetParentFrame) and, vice versa, main frame's parent is
  // always null.
  kSubframe,
  // Primary main frame is 1:1 with a WebContents object and owned by it.
  kPrimaryMainFrame,
  // This frame was created by prerendering to load a page before the user
  // navigated to it - then when the user navigates to the prerendered URL, the
  // page will be transferred to the primary main frame.
  kPrerenderMainFrame,
  // A root of an isolated frame tree created on behalf of a <fencedframe>
  // element.
  kFencedFrameRoot,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FRAME_TYPE_H_
