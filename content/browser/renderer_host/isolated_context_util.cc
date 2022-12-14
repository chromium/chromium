// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/isolated_context_util.h"

#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"

namespace content {

bool IsFrameSufficientlyIsolated(RenderFrameHost* frame) {
  if (frame->GetWebExposedIsolationLevel() >=
      content::RenderFrameHost::WebExposedIsolationLevel::
          kMaybeIsolatedApplication) {
    return true;
  }

  if (GetContentClient()->browser()->IsIsolatedContextAllowedForUrl(
          frame->GetBrowserContext(),
          frame->GetProcess()->GetProcessLock().lock_url())) {
    return true;
  }

  return false;
}

}  // namespace content
