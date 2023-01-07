// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PROCESS_VISIBILITY_UTIL_H_
#define CONTENT_PUBLIC_BROWSER_PROCESS_VISIBILITY_UTIL_H_

#include "content/common/content_export.h"

namespace content {

// Called by the browser when the browser process becomes visible or
// invisible.
CONTENT_EXPORT void OnBrowserVisibilityChanged(bool visible);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PROCESS_VISIBILITY_UTIL_H_
