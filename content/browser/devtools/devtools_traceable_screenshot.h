// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_TRACEABLE_SCREENSHOT_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_TRACEABLE_SCREENSHOT_H_

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/trace_event/trace_event_impl.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

class DevToolsTraceableScreenshot
    : public base::trace_event::ConvertableToTraceFormat {
 public:
  static constexpr int kMaximumNumberOfScreenshots = 450;

  static base::subtle::Atomic32 GetNumberOfInstances();

  DevToolsTraceableScreenshot(const SkBitmap& bitmap);

  ~DevToolsTraceableScreenshot() override;

  // base::trace_event::ConvertableToTraceFormat implementation.
  void AppendAsTraceFormat(std::string* out) const override;

 private:
  static base::subtle::Atomic32 number_of_instances_;

  SkBitmap frame_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsTraceableScreenshot);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_TRACEABLE_SCREENSHOT_H_
