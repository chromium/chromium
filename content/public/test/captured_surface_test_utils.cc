// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/captured_surface_test_utils.h"

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"

namespace content {

void DidCapturedSurfaceControlForTesting(WebContents* web_contents) {
  static_cast<WebContentsImpl*>(web_contents)->DidCapturedSurfaceControl();
}

}  // namespace content
