// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_HOST_ID_H_
#define CONTENT_BROWSER_PRERENDER_HOST_ID_H_

#include "base/types/id_type.h"
#include "content/common/content_export.h"

namespace content {

// A strongly-typed identifier for PrerenderHost.
using PrerenderHostId = base::IdTypeU64<class PrerenderHost>;

}  // namespace content

#endif  // CONTENT_BROWSER_PRERENDER_HOST_ID_H_
