// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_view_delegate.h"

#include <stddef.h>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "content/public/common/drop_data.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

WebContentsViewDelegate::~WebContentsViewDelegate() {
}

gfx::NativeWindow WebContentsViewDelegate::GetNativeWindow() {
  return gfx::NativeWindow();
}

WebDragDestDelegate* WebContentsViewDelegate::GetDragDestDelegate() {
  return nullptr;
}

void WebContentsViewDelegate::ShowContextMenu(
    RenderFrameHost& render_frame_host,
    const ContextMenuParams& params) {}

void WebContentsViewDelegate::DismissContextMenu() {}

void WebContentsViewDelegate::ExecuteCommandForTesting(int command_id,
                                                       int event_flags) {
  NOTREACHED_IN_MIGRATION();
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

void* WebContentsViewDelegate::GetDelegateForHost(
    RenderWidgetHost* render_widget_host,
    bool is_popup) {
  return nullptr;
}

void WebContentsViewDelegate::OnPerformingDrop(
    const DropData& drop_data,
    DropCompletionCallback callback) {
  return std::move(callback).Run(drop_data);
}

}  // namespace content
