// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/render_frame_host_test_support.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {

void LeaveInPendingDeletionState(RenderFrameHost* rfh) {
  static_cast<RenderFrameHostImpl*>(rfh)->DoNotDeleteForTesting();
}

}  // namespace content
