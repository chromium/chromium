// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_SELECTION_POPUP_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_SELECTION_POPUP_DELEGATE_H_

#include <memory>

#include "content/common/content_export.h"

namespace ui {
class MenuModel;
}  // namespace ui

namespace content {

class RenderFrameHost;
struct ContextMenuParams;

// Delegate used to customize behavior of SelectionPopupController in Android.
class CONTENT_EXPORT SelectionPopupDelegate {
 public:
  SelectionPopupDelegate() = default;
  SelectionPopupDelegate(const SelectionPopupDelegate&) = delete;
  SelectionPopupDelegate& operator=(const SelectionPopupDelegate&) = delete;

  virtual ~SelectionPopupDelegate();

  // Returns a MenuModel containing extra items to add at the bottom of the
  // selected text context menu.
  virtual std::unique_ptr<ui::MenuModel> GetSelectionPopupExtraItems(
      RenderFrameHost& render_frame_host,
      const ContextMenuParams& params);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_SELECTION_POPUP_DELEGATE_H_
