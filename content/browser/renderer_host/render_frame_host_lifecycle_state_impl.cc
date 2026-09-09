// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_lifecycle_state_impl.h"

#include <ostream>

namespace content {

const char* RenderFrameHostLifecycleStateImplToString(
    RenderFrameHostLifecycleStateImpl state) {
  switch (state) {
    case RenderFrameHostLifecycleStateImpl::kSpeculative:
      return "Speculative";
    case RenderFrameHostLifecycleStateImpl::kPendingCommit:
      return "PendingCommit";
    case RenderFrameHostLifecycleStateImpl::kPrerendering:
      return "Prerendering";
    case RenderFrameHostLifecycleStateImpl::kActive:
      return "Active";
    case RenderFrameHostLifecycleStateImpl::kInBackForwardCache:
      return "InBackForwardCache";
    case RenderFrameHostLifecycleStateImpl::kRunningUnloadHandlers:
      return "RunningUnloadHandlers";
    case RenderFrameHostLifecycleStateImpl::kReadyToBeDeleted:
      return "ReadyToBeDeleted";
  }
}

std::ostream& operator<<(std::ostream& o,
                         const RenderFrameHostLifecycleStateImpl& s) {
  return o << RenderFrameHostLifecycleStateImplToString(s);
}

}  // namespace content
