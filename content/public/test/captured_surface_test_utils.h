// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_CAPTURED_SURFACE_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_CAPTURED_SURFACE_TEST_UTILS_H_

#include "content/public/browser/web_contents.h"

namespace content {

// Expose WebContentsImpl::DidCapturedSurfaceControl() to tests
// outside of content/.
// TODO(crbug.com/332695392): Remove this function.
void DidCapturedSurfaceControlForTesting(WebContents* web_contents);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_CAPTURED_SURFACE_TEST_UTILS_H_
