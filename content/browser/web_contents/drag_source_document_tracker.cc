// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/drag_source_document_tracker.h"

#include "content/browser/web_contents/drag_source_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"

namespace content {

DOCUMENT_USER_DATA_KEY_IMPL(DragSourceDocumentTracker);

DragSourceDocumentTracker::DragSourceDocumentTracker(RenderFrameHost* rfh)
    : DocumentUserData<DragSourceDocumentTracker>(rfh) {}

DragSourceDocumentTracker::~DragSourceDocumentTracker() {
  if (drag_ids_.empty()) {
    return;
  }

  // Clean up in map if it still exists.
  BrowserContext* browser_context = render_frame_host().GetBrowserContext();
  if (auto* map = DragSourceMap::Get(browser_context)) {
    for (const auto& drag_id : drag_ids_) {
      map->RemoveDragSource(drag_id);
    }
  }
}

void DragSourceDocumentTracker::AddDragId(const WebContents::DragId& drag_id) {
  drag_ids_.insert(drag_id);
}

}  // namespace content
