// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACING_UI_H_
#define CONTENT_BROWSER_TRACING_TRACING_UI_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace base {
namespace trace_event {
class TraceConfig;
}  // namespace trace_event
}  // namespace base

namespace content {

class TracingUI;

// WebUIConfig for the chrome://tracing page.
class TracingUIConfig : public DefaultWebUIConfig<TracingUI> {
 public:
  TracingUIConfig()
      : DefaultWebUIConfig(kChromeUIScheme, kChromeUITracingHost) {}
};

// The C++ back-end for the chrome://tracing webui page.
class CONTENT_EXPORT TracingUI : public WebUIController {
 public:
  explicit TracingUI(WebUI* web_ui);

  TracingUI(const TracingUI&) = delete;
  TracingUI& operator=(const TracingUI&) = delete;

  ~TracingUI() override;

  // Public for testing.
  static bool GetTracingOptions(const std::string& data64,
                                base::trace_event::TraceConfig& trace_config,
                                std::string& out_stream_format);

 private:
  base::WeakPtrFactory<TracingUI> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACING_UI_H_
