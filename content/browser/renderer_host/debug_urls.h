// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DEBUG_URLS_H_
#define CONTENT_BROWSER_RENDERER_HOST_DEBUG_URLS_H_

#include "content/common/content_export.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace content {

// Returns true if |url| is a special debugging URL for triggering
// intentional browser behaviors like crashes or hangs.
CONTENT_EXPORT bool IsDebugURL(const GURL& url);

// Triggers debug action for |url| if |IsDebugURL| returns true for it;
// otherwise, this function will crash.
CONTENT_EXPORT void HandleDebugURL(const GURL& url,
                                   ui::PageTransition transition,
                                   bool is_explicit_navigation);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DEBUG_URLS_H_
