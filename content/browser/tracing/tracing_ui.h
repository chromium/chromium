// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACING_UI_H_
#define CONTENT_BROWSER_TRACING_TRACING_UI_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_controller.h"

namespace base {
namespace trace_event {
class TraceConfig;
}  // namespace trace_event
}  // namespace base

namespace content {

class TracingDelegate;

// The C++ back-end for the chrome://tracing webui page.
class CONTENT_EXPORT TracingUI : public WebUIController {
 public:
  explicit TracingUI(WebUI* web_ui);
  ~TracingUI() override;

  // Public for testing.
  static bool GetTracingOptions(const std::string& data64,
                                base::trace_event::TraceConfig* trace_config);


 private:
  std::unique_ptr<TracingDelegate> delegate_;
  base::WeakPtrFactory<TracingUI> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TracingUI);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACING_UI_H_
