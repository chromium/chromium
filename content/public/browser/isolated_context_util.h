// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ISOLATED_CONTEXT_UTIL_H_
#define CONTENT_PUBLIC_BROWSER_ISOLATED_CONTEXT_UTIL_H_

#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;
class RenderProcessHost;

// These functions check whether a frame or process is sufficiently isolated
// to have access to interfaces intended only for isolated contexts.
// See the isolated contexts spec:
// https://wicg.github.io/isolated-web-apps/isolated-contexts.html
// Isolated Web Apps Explainer:
// https://github.com/WICG/isolated-web-apps/blob/main/README.md

// Checks whether `frame` meets the requirements for qualifying as an isolated
// context, and is therefore allowed access to isolated context gated APIs.
//
// RenderFrameHost* could have a lower isolation level than its
// RenderProcessHost* because of the cross-origin-isolated permissions policy.
//
// This should be used to check for API access instead of IsIsolatedContext
// whenever possible.
CONTENT_EXPORT bool HasIsolatedContextCapability(RenderFrameHost* frame);

// Checks whether `process` meets the requirements for qualifying as an
// isolated context.
//
// HasIsolatedContextCapability should be used to check for API access instead
// of this function whenever possible. Shared/service workers should use this
// function because they don't have a RenderFrameHost, so the additional
// permissions policy check done by HasIsolatedContextCapability doesn't apply
// to them (permissions policy applies to documents).
CONTENT_EXPORT bool IsIsolatedContext(RenderProcessHost* process);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ISOLATED_CONTEXT_UTIL_H_
