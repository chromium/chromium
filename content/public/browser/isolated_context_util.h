// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ISOLATED_CONTEXT_UTIL_H_
#define CONTENT_PUBLIC_BROWSER_ISOLATED_CONTEXT_UTIL_H_

#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;
class RenderProcessHost;

// Whether the given frame is sufficiently isolated to have access
// to interfaces intended only for isolated contexts.
// See [IsolatedContext] IDL extended attribute for more details.
// Isolated Web Apps Explainer:
// https://github.com/WICG/isolated-web-apps/blob/main/README.md

// Checks whether the given `process` fulfills the necessary requirements for
// qualifying as an isolated context.
CONTENT_EXPORT bool IsIsolatedContext(RenderProcessHost* process);

// Checks whether the given `frame` fulfills the necessary requirements for
// qualifying as an isolated context.
// RenderFrameHost* could have a lower WebExposedIsolationLevel than its
// RenderProcessHost* because of the cross-origin-isolated permissions policy;
// that's why it's undesirable to delegate to `IsIsolatedContext()` via
// `frame->GetProcess()`.
CONTENT_EXPORT bool HasIsolatedContextCapability(RenderFrameHost* frame);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ISOLATED_CONTEXT_UTIL_H_
