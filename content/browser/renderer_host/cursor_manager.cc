// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/cursor_manager.h"

#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace content {

CursorManager::CursorManager(RenderWidgetHostViewBase* root)
    : view_under_cursor_(root), root_view_(root) {}

CursorManager::~CursorManager() {}

void CursorManager::UpdateCursor(RenderWidgetHostViewBase* view,
                                 const ui::Cursor& cursor) {
  cursor_map_[view] = cursor;
  if (view == view_under_cursor_)
    root_view_->DisplayCursor(cursor);
}

void CursorManager::UpdateViewUnderCursor(RenderWidgetHostViewBase* view) {
  if (view == view_under_cursor_)
    return;

  // Whenever we switch from one view to another, clear the tooltip: as the
  // mouse moves, the view now controlling the cursor will send a new tooltip,
  // though this is only guaranteed if the view's tooltip is non-empty, so
  // clearing here is important. Tooltips sent from the previous view will be
  // ignored.
  root_view_->UpdateTooltip(std::u16string());
  view_under_cursor_ = view;
  ui::Cursor cursor(ui::mojom::CursorType::kPointer);

  auto it = cursor_map_.find(view);
  if (it != cursor_map_.end())
    cursor = it->second;

  root_view_->DisplayCursor(cursor);
}

void CursorManager::ViewBeingDestroyed(RenderWidgetHostViewBase* view) {
  cursor_map_.erase(view);

  // If the view right under the mouse is going away, use the root's cursor
  // until UpdateViewUnderCursor is called again.
  if (view == view_under_cursor_ && view != root_view_)
    UpdateViewUnderCursor(root_view_);
}

bool CursorManager::IsViewUnderCursor(RenderWidgetHostViewBase* view) const {
  return view == view_under_cursor_;
}

bool CursorManager::GetCursorForTesting(RenderWidgetHostViewBase* view,
                                        ui::Cursor& cursor) {
  if (cursor_map_.find(view) == cursor_map_.end())
    return false;

  cursor = cursor_map_[view];
  return true;
}

}  // namespace content
