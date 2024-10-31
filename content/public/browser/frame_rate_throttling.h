// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FRAME_RATE_THROTTLING_H_
#define CONTENT_PUBLIC_BROWSER_FRAME_RATE_THROTTLING_H_

#include <set>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

// Signals the frame sink manager that all current and future frame sinks should
// start sending BeginFrames at an interval that is at least as long `interval`.
// Because there is a single viz process, which itself contains a single host
// frame sink manager, calling this function multiple times from anywhere will
// apply the throttling described by the latest call. For instance:
//
// Foo calling StartThrottlingAllFrameSinks(Hertz(15));
//
// followed by
//
// Bar calling StartThrottlingAllFrameSinks(Hertz(30));
//
// Will result in framerate being throttled at 30hz
// Should be called from the UI thread.
CONTENT_EXPORT void StartThrottlingAllFrameSinks(base::TimeDelta interval);

// Stops the BeginFrame throttling enabled by `StartThrottlingAllFrameSinks()`.
// Should be called from the UI thread.
CONTENT_EXPORT void StopThrottlingAllFrameSinks();

// Gets the frame sink id list from |throttle_frames|, and signals the frame
// sink manager to throttle the frame sinks specified and all their descendant
// sinks to send BeginFrames at an interval of |interval|. This operation clears
// out any previous throttling operation on any frame sinks.
//
// Note that if global throttling (like StartThrottlingAllFrameSinks invoked) is
// enabled, per-frame sink throttling with the same interval doesn't take
// effect. Per-frame sink throttling with more aggressive interval would apply
// on top of global throttling.
// Should be called from the UI thread.
CONTENT_EXPORT void UpdateThrottlingFrameSinks(
    const std::set<GlobalRenderFrameHostId>& throttle_frames,
    base::TimeDelta interval);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FRAME_RATE_THROTTLING_H_
