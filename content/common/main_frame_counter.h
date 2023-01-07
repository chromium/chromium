// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MAIN_FRAME_COUNTER_H_
#define CONTENT_COMMON_MAIN_FRAME_COUNTER_H_

#include <stddef.h>

#include "base/component_export.h"

namespace content {

// This should only be used from the Renderer process, but is placed in common
// so the Browser process can access it for testing only.
//
// This keeps track of how many main frames exist in the current Renderer
// process.
//
// This is for an ongoing experiment to reduce memory usage in Renderers that
// only contain subframes; it should be removed if the experiment does not
// end up shipping. See: crbug.com/1331368 for tracking.
class COMPONENT_EXPORT(MAIN_FRAME_COUNTER) MainFrameCounter final {
 public:
  static bool has_main_frame();

 private:
  friend class RenderFrameImpl;

  static void IncrementCount();
  static void DecrementCount();

  static size_t main_frame_count_;
};

}  // namespace content

#endif  // CONTENT_COMMON_MAIN_FRAME_COUNTER_H_
