// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_WEB_CONTENTS_ACCESSIBILITY_H_
#define CONTENT_BROWSER_ACCESSIBILITY_WEB_CONTENTS_ACCESSIBILITY_H_

#include <stdint.h>

#include "content/common/content_export.h"

namespace content {

// Class that bridges BrowserAccessibilityManager and platform-dependent
// handler.
// TODO(crbug.com/40522979): Expand this class to work on all the platforms.
class CONTENT_EXPORT WebContentsAccessibility {
 public:
  WebContentsAccessibility() {}

  WebContentsAccessibility(const WebContentsAccessibility&) = delete;
  WebContentsAccessibility& operator=(const WebContentsAccessibility&) = delete;

  virtual ~WebContentsAccessibility() {}
};
}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_WEB_CONTENTS_ACCESSIBILITY_H_
