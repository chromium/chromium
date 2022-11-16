// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_CURSOR_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_CURSOR_MANAGER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"

namespace content {

class RenderWidgetHostViewBase;

// CursorManager coordinates mouse cursors for multiple RenderWidgetHostViews
// on a single page. It is owned by the top-level RenderWidgetHostView and
// calls back to its DisplayCursor method when the cursor needs to change,
// either because the mouse moved over a different view or because a cursor
// update was received for the current view.
class CONTENT_EXPORT CursorManager {
 public:

  CursorManager(RenderWidgetHostViewBase* root);
  ~CursorManager();

  // Called for any RenderWidgetHostView that received an UpdateCursor message
  // from its renderer process.
  void UpdateCursor(RenderWidgetHostViewBase*, const WebCursor&);

  // Called when the mouse moves over a different RenderWidgetHostView.
  void UpdateViewUnderCursor(RenderWidgetHostViewBase*);

  // Notification of a RenderWidgetHostView being destroyed, so that its
  // cursor map entry can be removed if it has one. If it is the current
  // view_under_cursor_, then the root_view_'s cursor will be displayed.
  void ViewBeingDestroyed(RenderWidgetHostViewBase*);

  // Called by any RenderWidgetHostView before updating the tooltip text to
  // validate that the tooltip text being updated is for the view under the
  // cursor. This is only used for cursor triggered tooltips.
  bool IsViewUnderCursor(RenderWidgetHostViewBase*) const;

  // Accessor for browser tests, enabling verification of the cursor_map_.
  // Returns false if the provided View is not in the map, and outputs
  // the cursor otherwise.
  bool GetCursorForTesting(RenderWidgetHostViewBase*, WebCursor&);

 private:
  // Stores the last received cursor from each RenderWidgetHostView.
  std::map<RenderWidgetHostViewBase*, WebCursor> cursor_map_;

  // The view currently underneath the cursor, which corresponds to the cursor
  // currently displayed.
  raw_ptr<RenderWidgetHostViewBase, DanglingUntriaged> view_under_cursor_;

  // The root view is the target for DisplayCursor calls whenever the active
  // cursor needs to change.
  raw_ptr<RenderWidgetHostViewBase> root_view_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_CURSOR_MANAGER_H_
