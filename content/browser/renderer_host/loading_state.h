// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_LOADING_STATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_LOADING_STATE_H_

namespace content {

// Used to specify the loading state of a frame, or the frame tree as a whole.
enum class LoadingState {
  // Not currently loading.
  NONE,
  // Loading currently in progress, but no loading UI should be shown. Used
  // for most same-document navigations.
  LOADING_WITHOUT_UI,
  // Loading currently in progress, and loading UI is recommended. This is
  // used for cross-document navigations, as well as asynchronous same-document
  // navigations from the web-exposed navigation API. Note that even if a
  // FrameTreeNode's LoadingState is LOADING_UI_REQUESTED, the FrameTree may
  // decide the tree-wide policy is LOADING_WITHOUT_UI if the root frame is not
  // loading. Also, the embedder is under no obligation to showing any UI.
  LOADING_UI_REQUESTED,
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_LOADING_STATE_H_
