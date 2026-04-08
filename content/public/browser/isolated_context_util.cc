// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/isolated_context_util.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_exposed_isolation_level.h"

namespace content {

bool HasIsolatedContextCapability(RenderFrameHost* frame) {
  return frame->GetWebExposedIsolationLevel() ==
         WebExposedIsolationLevel::kIsolatedApplication;
}

bool IsIsolatedContext(RenderProcessHost* process) {
  return process->GetWebExposedIsolationLevel() ==
         WebExposedIsolationLevel::kIsolatedApplication;
}

}  // namespace content
