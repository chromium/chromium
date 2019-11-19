// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_view_delegate.h"

#include <stddef.h>
#include <utility>

#include "base/callback.h"
#include "content/public/common/drop_data.h"

namespace content {

WebContentsViewDelegate::~WebContentsViewDelegate() {
}

gfx::NativeWindow WebContentsViewDelegate::GetNativeWindow() {
  return nullptr;
}

WebDragDestDelegate* WebContentsViewDelegate::GetDragDestDelegate() {
  return nullptr;
}

void WebContentsViewDelegate::ShowContextMenu(
    RenderFrameHost* render_frame_host,
    const ContextMenuParams& params) {
}

void WebContentsViewDelegate::StoreFocus() {
}

bool WebContentsViewDelegate::RestoreFocus() {
  return false;
}

void WebContentsViewDelegate::ResetStoredFocus() {}

bool WebContentsViewDelegate::Focus() {
  return false;
}

bool WebContentsViewDelegate::TakeFocus(bool reverse) {
  return false;
}

void* WebContentsViewDelegate::CreateRenderWidgetHostViewDelegate(
    RenderWidgetHost* render_widget_host,
    bool is_popup) {
  return nullptr;
}

void WebContentsViewDelegate::OnPerformDrop(const DropData& drop_data,
                                            DropCompletionCallback callback) {
  return std::move(callback).Run(DropCompletionResult::kContinue);
}

}  // namespace content
