// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRAME_TRACE_RECORDER_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRAME_TRACE_RECORDER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace cc {
class RenderFrameMetadata;
}

namespace content {

class RenderFrameHostImpl;

class DevToolsFrameTraceRecorder {
 public:
  DevToolsFrameTraceRecorder();
  ~DevToolsFrameTraceRecorder();

  void OnSynchronousSwapCompositorFrame(
      RenderFrameHostImpl* host,
      const cc::RenderFrameMetadata& metadata);

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsFrameTraceRecorder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRAME_TRACE_RECORDER_H_
