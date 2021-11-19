// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATING_FRAME_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATING_FRAME_TYPE_H_

#include "content/common/content_export.h"

namespace content {

// Defines the type of frame in which a navigation is taking place. (See below
// for more details.)
enum NavigatingFrameType {
  // The frame type when the navigation is taking place in the main frame of the
  // primary frame tree.
  // See the description of `NavigationHandle::IsInPrimaryMainFrame` for
  // details.
  kPrimaryMainFrame,
  // The frame type when the navigation is taking place in the main frame of the
  // prerendered frame tree. It doesn't cover prerender activations, which are
  // considered to be kPrimaryMainFrame.
  // See the description of `NavigationHandle::IsInPrerenderedMainFrame` for
  // details.
  kPrerenderMainFrame,
  // The frame type when the navigation is taking place in the root frame of the
  // fenced frame tree.
  // See the description of `RenderFrameHost::IsFencedFrameRoot` for details.
  kFencedFrameRoot,
  // The frame type when the navigation is taking place in a subframe.
  kSubframe,
  // TODO(crbug.com/1267506): Consider if we need to have the types for portals
  // or guestviews.
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATING_FRAME_TYPE_H_
