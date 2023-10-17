// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_CONTENTS_H_
#define EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_CONTENTS_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/app_window/app_window.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}

namespace extensions {

// AppWindowContents class specific to app windows. It maintains a
// WebContents instance and observes it for the purpose of passing
// messages to the extensions system.
class AppWindowContentsImpl : public AppWindowContents,
                              public content::WebContentsObserver {
 public:
  explicit AppWindowContentsImpl(AppWindow* host);

  AppWindowContentsImpl(const AppWindowContentsImpl&) = delete;
  AppWindowContentsImpl& operator=(const AppWindowContentsImpl&) = delete;

  ~AppWindowContentsImpl() override;

  // AppWindowContents
  void Initialize(content::BrowserContext* context,
                  content::RenderFrameHost* creator_frame,
                  const GURL& url) override;
  void LoadContents(int32_t creator_process_id) override;
  void NativeWindowChanged(NativeAppWindow* native_app_window) override;
  void NativeWindowClosed(bool send_onclosed) override;
  content::WebContents* GetWebContents() const override;
  WindowController* GetWindowController() const override;

 private:
  // content::WebContentsObserver
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  raw_ptr<AppWindow> host_;  // This class is owned by |host_|
  GURL url_;
  std::unique_ptr<content::WebContents> web_contents_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_CONTENTS_H_
