// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/selection_popup_delegate.h"

#include "ui/base/models/menu_model.h"

namespace content {

class RenderFrameHost;
struct ContextMenuParams;

SelectionPopupDelegate::~SelectionPopupDelegate() = default;

std::unique_ptr<ui::MenuModel>
SelectionPopupDelegate::GetSelectionPopupExtraItems(
    RenderFrameHost& render_frame_host,
    const ContextMenuParams& params) {
  return nullptr;
}

}  // namespace content
