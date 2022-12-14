// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_ISOLATED_CONTEXT_UTIL_H_
#define CONTENT_BROWSER_RENDERER_HOST_ISOLATED_CONTEXT_UTIL_H_

#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;

// Whether the given frame is sufficiently isolated to have access
// to interfaces intended only for isolated contexts.
// See [IsolatedContext] IDL extended attribute for more details.
// Isolated Web Apps Explainer:
// https://github.com/WICG/isolated-web-apps/blob/main/README.md
CONTENT_EXPORT bool IsFrameSufficientlyIsolated(RenderFrameHost* frame);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_ISOLATED_CONTEXT_UTIL_H_
