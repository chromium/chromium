// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_UI_WEB_CONTENTS_VIEW_DELEGATE_IMPL_H_
#define IOS_WEB_CONTENT_UI_WEB_CONTENTS_VIEW_DELEGATE_IMPL_H_

#import "base/memory/raw_ptr.h"
#import "content/public/browser/web_contents_view_delegate.h"

namespace content {
class WebContents;
class WebContentsViewDelegate;
}  // namespace content

class WebContentsViewDelegateImpl : public content::WebContentsViewDelegate {
 public:
  explicit WebContentsViewDelegateImpl(content::WebContents* web_contents);

  WebContentsViewDelegateImpl(const WebContentsViewDelegateImpl&) = delete;
  WebContentsViewDelegateImpl& operator=(const WebContentsViewDelegateImpl&) =
      delete;

  ~WebContentsViewDelegateImpl() override;

  // WebContentsViewDelegate:
  void ShowContextMenu(content::RenderFrameHost& render_frame_host,
                       const content::ContextMenuParams& params) override;
  void DismissContextMenu() override;

 private:
  // The WebContents that owns the view and this delegate transitively.
  raw_ptr<content::WebContents> web_contents_;
};

std::unique_ptr<content::WebContentsViewDelegate> CreateWebContentsViewDelegate(
    content::WebContents* web_contents);

#endif  // IOS_WEB_CONTENT_UI_WEB_CONTENTS_VIEW_DELEGATE_IMPL_H_
