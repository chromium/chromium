// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRERENDER_HOST_ID_H_
#define CONTENT_PUBLIC_BROWSER_PRERENDER_HOST_ID_H_

#include "base/types/id_type.h"
#include "content/common/content_export.h"

namespace content {

// A strongly-typed identifier for PrerenderHost.
// -1 is used as the invalid value to maintain compatibility with Android
// WebView APIs which return -1 on failure. 1 is the first generated valid ID.
using PrerenderHostId = base::IdType<class PrerenderHost, int64_t, -1, 1>;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRERENDER_HOST_ID_H_
