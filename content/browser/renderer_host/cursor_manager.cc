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

CursorManager::~CursorManager() = default;

void CursorManager::UpdateCursor(RenderWidgetHostViewBase* view,
                                 const ui::Cursor& cursor) {
  cursor_map_[view] = cursor;
  if (view == view_under_cursor_) {
    UpdateCursor();
  }
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
  UpdateCursor();
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

base::ScopedClosureRunner CursorManager::CreateDisallowCustomCursorScope() {
  bool should_update_cursor = false;

  // If custom cursors are about to be disallowed and the current view uses a
  // custom cursor, the cursor needs to be updated to replace the custom cursor.
  if (AreCustomCursorsAllowed() && cursor_map_[view_under_cursor_].type() ==
                                       ui::mojom::CursorType::kCustom) {
    should_update_cursor = true;
  }

  ++disallow_custom_cursor_scope_count_;

  if (should_update_cursor) {
    UpdateCursor();
  }

  return base::ScopedClosureRunner(
      base::BindOnce(&CursorManager::DisallowCustomCursorScopeExpired,
                     weak_factory_.GetWeakPtr()));
}

bool CursorManager::GetCursorForTesting(RenderWidgetHostViewBase* view,
                                        ui::Cursor& cursor) {
  if (cursor_map_.find(view) == cursor_map_.end()) {
    return false;
  }

  cursor = cursor_map_[view];
  return true;
}

bool CursorManager::AreCustomCursorsAllowed() const {
  return disallow_custom_cursor_scope_count_ == 0;
}

void CursorManager::DisallowCustomCursorScopeExpired() {
  --disallow_custom_cursor_scope_count_;

  // If custom cursors started being allowed and the current view has a custom
  // cursor, update the cursor to ensure the custom cursor is now displayed.
  if (AreCustomCursorsAllowed() && cursor_map_[view_under_cursor_].type() ==
                                       ui::mojom::CursorType::kCustom) {
    UpdateCursor();
  }
}

void CursorManager::UpdateCursor() {
  ui::Cursor cursor(ui::mojom::CursorType::kPointer);

  auto it = cursor_map_.find(view_under_cursor_);
  if (it != cursor_map_.end() &&
      (AreCustomCursorsAllowed() ||
       it->second.type() != ui::mojom::CursorType::kCustom)) {
    cursor = it->second;
  }

  last_set_cursor_type_for_testing_ = cursor.type();

  root_view_->DisplayCursor(cursor);
}

}  // namespace content
