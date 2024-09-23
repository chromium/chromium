// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/isolated_context_util.h"

#include "content/browser/process_lock.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_exposed_isolation_level.h"
#include "content/public/common/content_client.h"

namespace content {

namespace {

bool IsIsolatedContextAllowedByEmbedder(RenderProcessHost* process) {
  const ProcessLock& process_lock = process->GetProcessLock();
  return !process_lock.is_sandboxed() &&
         GetContentClient()->browser()->IsIsolatedContextAllowedForUrl(
             process->GetBrowserContext(), process_lock.lock_url());
}

}  // namespace

bool HasIsolatedContextCapability(RenderFrameHost* frame) {
  return (frame->GetWebExposedIsolationLevel() ==
          WebExposedIsolationLevel::kIsolatedApplication) ||
         IsIsolatedContextAllowedByEmbedder(frame->GetProcess());
}

bool IsIsolatedContext(RenderProcessHost* process) {
  return (process->GetWebExposedIsolationLevel() ==
          WebExposedIsolationLevel::kIsolatedApplication) ||
         IsIsolatedContextAllowedByEmbedder(process);
}

}  // namespace content
