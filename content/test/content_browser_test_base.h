// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_CONTENT_BROWSER_TEST_BASE_H_
#define CONTENT_TEST_CONTENT_BROWSER_TEST_BASE_H_

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"

namespace content {

class FrameTreeNode;
class RenderFrameHostImpl;

// A generic base class for content-internal browsertests. This includes basic
// utility methods common in many content browser tests.
class ContentBrowserTestBase : public ContentBrowserTest {
 protected:
  // Starts an `embedded_test_server` with a `host_resolver`.
  void SetUpOnMainThread() override;

  // Returns the WebContents within `shell()`.
  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  // Returns the primary main frame's FrameTreeNode from `web_contents()`.
  FrameTreeNode* main_frame() const {
    return web_contents()->GetPrimaryFrameTree().root();
  }

  // Returns the current RenderFrameHostImpl for `main_frame()`.
  RenderFrameHostImpl* main_frame_host() const {
    return main_frame()->current_frame_host();
  }
};

}  // namespace content

#endif  // CONTENT_TEST_CONTENT_BROWSER_TEST_BASE_H_
