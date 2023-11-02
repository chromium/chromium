// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FRAME_RATE_THROTTLING_H_
#define CONTENT_PUBLIC_BROWSER_FRAME_RATE_THROTTLING_H_

#include "base/time/time.h"
#include "content/common/content_export.h"

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

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FRAME_RATE_THROTTLING_H_
