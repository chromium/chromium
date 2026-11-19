// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_guard.h"

#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

WebContentsGuard::WebContentsGuard(WebContents& contents)
    : guard_(&static_cast<WebContentsImpl&>(contents).prevent_destruction_,
             true) {}
WebContentsGuard::~WebContentsGuard() = default;

}  // namespace content
