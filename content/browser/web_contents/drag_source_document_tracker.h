// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_DRAG_SOURCE_DOCUMENT_TRACKER_H_
#define CONTENT_BROWSER_WEB_CONTENTS_DRAG_SOURCE_DOCUMENT_TRACKER_H_

#include "content/public/browser/document_user_data.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace content {

class RenderFrameHost;

// DragSourceDocumentTracker tracks active drag IDs associated with a specific
// document instance.
//
// Since `RenderFrameHost` (RFH) lifecycles can outlive individual documents
// (e.g., during same-site navigations), checking only the originating RFH is
// insufficient to verify drag provenance.
//
// By subclassing `DocumentUserData`, this tracker's lifecycle is strictly bound
// to the single document instance loaded in the frame. If the frame navigates
// to a new document, the previous document's tracker is destroyed. When
// verifying drag provenance (e.g., in `WebContents::FromDragId`), we verify
// that the original initiating document is still live and holds the drag ID.
// This prevents spoofing and cross-document origin leaks if a frame navigates
// while a drag is in progress.
class DragSourceDocumentTracker
    : public DocumentUserData<DragSourceDocumentTracker> {
 public:
  ~DragSourceDocumentTracker() override;

  void AddDragId(const WebContents::DragId& drag_id);
  bool has_drag_id(const WebContents::DragId& drag_id) const {
    return drag_ids_.contains(drag_id);
  }

 private:
  friend class DocumentUserData<DragSourceDocumentTracker>;
  DOCUMENT_USER_DATA_KEY_DECL();

  explicit DragSourceDocumentTracker(RenderFrameHost* rfh);

  absl::flat_hash_set<WebContents::DragId> drag_ids_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_DRAG_SOURCE_DOCUMENT_TRACKER_H_
