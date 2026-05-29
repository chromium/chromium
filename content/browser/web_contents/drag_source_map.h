// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_DRAG_SOURCE_MAP_H_
#define CONTENT_BROWSER_WEB_CONTENTS_DRAG_SOURCE_MAP_H_

#include "base/supports_user_data.h"
#include "base/unguessable_token.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {

class BrowserContext;

// DragSourceMap maintains a profile-wide registry of active drag operations,
// mapping a unique type-safe `WebContents::DragId` to the originating
// frame's `GlobalRenderFrameHostToken`.
//
// As a subclass of `base::SupportsUserData::Data` attached directly to a
// `BrowserContext`, the map's lifecycle and registry are strictly isolated per
// browser profile. This enforces profile-boundary validation: lookups for a
// drag ID initiated in a different profile will return an empty token,
// preventing cross-profile drag-and-drop data leaks.
class DragSourceMap : public base::SupportsUserData::Data {
 public:
  DragSourceMap();
  ~DragSourceMap() override;

  DragSourceMap(const DragSourceMap&) = delete;
  DragSourceMap& operator=(const DragSourceMap&) = delete;

  static DragSourceMap* Get(BrowserContext* browser_context);
  static DragSourceMap* GetOrCreate(BrowserContext* browser_context);

  void SetDragSource(const WebContents::DragId& drag_id,
                     const GlobalRenderFrameHostToken& source_rfh_token);
  GlobalRenderFrameHostToken GetDragSource(
      const WebContents::DragId& drag_id) const;
  void RemoveDragSource(const WebContents::DragId& drag_id);

 private:
  absl::flat_hash_map<WebContents::DragId, GlobalRenderFrameHostToken>
      drag_source_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_DRAG_SOURCE_MAP_H_
