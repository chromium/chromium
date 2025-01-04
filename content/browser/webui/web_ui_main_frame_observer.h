// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_MAIN_FRAME_OBSERVER_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_MAIN_FRAME_OBSERVER_H_

#include <stdint.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"

namespace blink::mojom {
enum class ConsoleMessageLevel;
}  // namespace blink::mojom

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
class WebUIImpl;

// The WebContentObserver for WebUIImpl. Each WebUIImpl has exactly one
// WebUIMainFrameObserver to watch for notifications from the associated
// WebContents object.
class CONTENT_EXPORT WebUIMainFrameObserver : public WebContentsObserver {
 public:
  WebUIMainFrameObserver(WebUIImpl* web_ui, WebContents* contents);
  ~WebUIMainFrameObserver() override;
  WebUIMainFrameObserver(const WebUIMainFrameObserver& rhs) = delete;
  WebUIMainFrameObserver& operator=(const WebUIMainFrameObserver& rhs) = delete;

 protected:
  friend class WebUIMainFrameObserverTest;

  // Override from WebContentsObserver
  void PrimaryPageChanged(Page& page) override;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
  // On official Google builds, capture and report JavaScript error messages on
  // WebUI surfaces back to Google. This allows us to fix JavaScript errors and
  // exceptions.
  void OnDidAddMessageToConsole(
      RenderFrameHost* source_frame,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const std::optional<std::u16string>& untrusted_stack_trace) override;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;

 private:
  void MaybeEnableWebUIJavaScriptErrorReporting(
      NavigationHandle* navigation_handle);

  // Do we report JavaScript errors ?
  bool error_reporting_enabled_ = false;

  raw_ptr<WebUIImpl> web_ui_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_MAIN_FRAME_OBSERVER_H_
