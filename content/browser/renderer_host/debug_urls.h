// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DEBUG_URLS_H_
#define CONTENT_BROWSER_RENDERER_HOST_DEBUG_URLS_H_

#include "ui/base/page_transition_types.h"

class GURL;

namespace content {

// Checks if the given url is a url used for debugging purposes, and if so
// handles it and returns true.
bool HandleDebugURL(const GURL& url,
                    ui::PageTransition transition,
                    bool is_explicit_navigation);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DEBUG_URLS_H_
