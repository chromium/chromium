// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/ui/web_contents_view_delegate_impl.h"

#import <memory>

#import "content/public/browser/context_menu_params.h"
#import "content/public/browser/web_contents.h"
#import "content/public/browser/web_contents_view_delegate.h"

WebContentsViewDelegateImpl::WebContentsViewDelegateImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

WebContentsViewDelegateImpl::~WebContentsViewDelegateImpl() {}

void WebContentsViewDelegateImpl::ShowContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  NOTIMPLEMENTED();
}

void WebContentsViewDelegateImpl::DismissContextMenu() {
  NOTIMPLEMENTED();
}

std::unique_ptr<content::WebContentsViewDelegate> CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return std::make_unique<WebContentsViewDelegateImpl>(web_contents);
}
