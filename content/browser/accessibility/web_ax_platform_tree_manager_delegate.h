// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_WEB_AX_PLATFORM_TREE_MANAGER_DELEGATE_H_
#define CONTENT_BROWSER_ACCESSIBILITY_WEB_AX_PLATFORM_TREE_MANAGER_DELEGATE_H_

#include "content/common/content_export.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"

namespace content {

class RenderFrameHostImpl;
class WebContentsAccessibility;

// Pure abstract class that is used by `BrowserAccessibilityManager` to gather
// information or perform actions that are implemented differently between the
// Web Content and the Views layers.
//
// Important: `BrowserAccessibilityManager` should never cache any of the
// returned pointers from any of these methods.
class CONTENT_EXPORT WebAXPlatformTreeManagerDelegate
    : public ui::AXPlatformTreeManagerDelegate {
 public:
  ~WebAXPlatformTreeManagerDelegate() override = default;
  WebAXPlatformTreeManagerDelegate(const WebAXPlatformTreeManagerDelegate&) =
      delete;
  WebAXPlatformTreeManagerDelegate& operator=(
      const WebAXPlatformTreeManagerDelegate&) = delete;

  // Returns true if this delegate represents the root (topmost) frame in a
  // tree of iframes on a webpage.
  virtual bool AccessibilityIsRootFrame() const = 0;

  virtual RenderFrameHostImpl* AccessibilityRenderFrameHost() = 0;
  virtual WebContentsAccessibility*
  AccessibilityGetWebContentsAccessibility() = 0;

 protected:
  WebAXPlatformTreeManagerDelegate() = default;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_WEB_AX_PLATFORM_TREE_MANAGER_DELEGATE_H_
