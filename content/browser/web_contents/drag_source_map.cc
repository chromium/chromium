// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/drag_source_map.h"

#include "content/public/browser/browser_context.h"

namespace content {

namespace {
const char kDragSourceMapUserDataKey[] = "content_drag_source_map";
}

DragSourceMap::DragSourceMap() = default;
DragSourceMap::~DragSourceMap() = default;

// static
DragSourceMap* DragSourceMap::Get(BrowserContext* browser_context) {
  DCHECK(browser_context);
  return static_cast<DragSourceMap*>(
      browser_context->GetUserData(kDragSourceMapUserDataKey));
}

// static
DragSourceMap* DragSourceMap::GetOrCreate(BrowserContext* browser_context) {
  DCHECK(browser_context);
  auto* map = static_cast<DragSourceMap*>(
      browser_context->GetUserData(kDragSourceMapUserDataKey));
  if (!map) {
    auto new_map = std::make_unique<DragSourceMap>();
    map = new_map.get();
    browser_context->SetUserData(kDragSourceMapUserDataKey, std::move(new_map));
  }
  return map;
}

void DragSourceMap::SetDragSource(
    const WebContents::DragId& drag_id,
    const GlobalRenderFrameHostToken& source_rfh_token) {
  drag_source_map_[drag_id] = source_rfh_token;
}

GlobalRenderFrameHostToken DragSourceMap::GetDragSource(
    const WebContents::DragId& drag_id) const {
  auto it = drag_source_map_.find(drag_id);
  if (it != drag_source_map_.end()) {
    return it->second;
  }
  return GlobalRenderFrameHostToken();
}

void DragSourceMap::RemoveDragSource(const WebContents::DragId& drag_id) {
  drag_source_map_.erase(drag_id);
}

}  // namespace content
