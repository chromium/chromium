// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_APP_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_APP_DELEGATE_H_

#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/app_window/app_delegate.h"

namespace extensions {

// AppDelegate implementation for app_shell. Sets focus after the WebContents is
// created. Ignores most operations that would create a new dialog or window.
class ShellAppDelegate : public AppDelegate {
 public:
  ShellAppDelegate();

  ShellAppDelegate(const ShellAppDelegate&) = delete;
  ShellAppDelegate& operator=(const ShellAppDelegate&) = delete;

  ~ShellAppDelegate() override;

  // AppDelegate overrides:
  void InitWebContents(content::WebContents* web_contents) override;
  void RenderFrameCreated(content::RenderFrameHost* frame_host) override;
  void ResizeWebContents(content::WebContents* web_contents,
                         const gfx::Size& size) override;
  content::WebContents* OpenURLFromTab(
      content::BrowserContext* context,
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  void AddNewContents(content::BrowserContext* context,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const blink::mojom::WindowFeatures& window_features,
                      bool user_gesture) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void RequestMediaAccessPermission(content::WebContents* web_contents,
                                    const content::MediaStreamRequest& request,
                                    content::MediaResponseCallback callback,
                                    const Extension* extension) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type,
                                  const Extension* extension) override;
  int PreferredIconSize() const override;
  void SetWebContentsBlocked(content::WebContents* web_contents,
                             bool blocked) override;
  bool IsWebContentsVisible(content::WebContents* web_contents) override;
  void SetTerminatingCallback(base::OnceClosure callback) override;
  void OnHide() override {}
  void OnShow() override {}
  bool TakeFocus(content::WebContents* web_contents, bool reverse) override;
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) override;
  void ExitPictureInPicture() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_APP_DELEGATE_H_
