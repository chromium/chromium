// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_MAIN_FRAME_OBSERVER_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_MAIN_FRAME_OBSERVER_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"  // FRIEND_TEST_ALL_PREFIXES
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"

namespace blink {
namespace mojom {
enum class ConsoleMessageLevel;
}
}  // namespace blink

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
class WebUIImpl;

class CONTENT_EXPORT WebUIMainFrameObserver : public WebContentsObserver {
 public:
  WebUIMainFrameObserver(WebUIImpl* web_ui, WebContents* contents);
  ~WebUIMainFrameObserver() override;
  WebUIMainFrameObserver(const WebUIMainFrameObserver& rhs) = delete;
  WebUIMainFrameObserver& operator=(const WebUIMainFrameObserver& rhs) = delete;

 protected:
  friend class WebUIMainFrameObserverTest;

  // Override from WebContentsObserver
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

// TODO(crbug.com/1129544) This is currently disabled due to Windows DLL
// thunking issues. Fix & re-enable.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // On official Google builds, capture and report JavaScript error messages on
  // WebUI surfaces back to Google. This allows us to fix JavaScript errors and
  // exceptions.
  void OnDidAddMessageToConsole(RenderFrameHost* source_frame,
                                blink::mojom::ConsoleMessageLevel log_level,
                                const base::string16& message,
                                int32_t line_no,
                                const base::string16& source_id) override;
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

 private:
  WebUIImpl* web_ui_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_MAIN_FRAME_OBSERVER_H_
