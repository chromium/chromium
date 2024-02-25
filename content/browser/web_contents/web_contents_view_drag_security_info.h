// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_DRAG_SECURITY_INFO_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_DRAG_SECURITY_INFO_H_

#include "content/browser/site_instance_group.h"

namespace content {

struct DropData;
class RenderWidgetHostImpl;

// Used to track security-salient details about a drag source. This class is to
// be owned by a WebContentsView indirectly via the Aura or Mac impls. Because
// the security concerns that it addresses are those that arise when the
// WebContentsView that initiated the drag is also the target of that drag, this
// class's main question is "is the WebContentsView that is receiving the drag
// the same one that initiated that drag?" The answer to that question may be
// directly obtained via `did_initiate()`, and that answer will affect every
// other member function's return value.
class WebContentsViewDragSecurityInfo {
 public:
  WebContentsViewDragSecurityInfo();
  ~WebContentsViewDragSecurityInfo();

  // Calls to be made by the owner to indicate that it has initiated or ended a
  // drag.
  void OnDragInitiated(RenderWidgetHostImpl* source_rwh,
                       const DropData& drop_data);
  void OnDragEnded();

  // Returns true iff the current drag was initiated by this WebContentsView.
  // This will be false for drags originating from other WebContentsViews or
  // from outside of the browser, as well as if there is no current drag.
  bool did_initiate() const { return did_initiate_; }

  // Returns whether the image on the drag is accessible. See
  // https://crbug.com/1264873.
  bool IsImageAccessibleFromFrame() const;

  // Returns whether `target_rwh` is a valid RenderWidgetHost to be dragging
  // over. This enforces that same-page, cross-site drags are not allowed. See
  // https://crbug.com/666858, https://crbug.com/1266953,
  // https://crbug.com/1485266.
  bool IsValidDragTarget(RenderWidgetHostImpl* target_rwh) const;

 private:
  // See `did_initiate()`, above.
  bool did_initiate_ = false;

  // The site instance of the drag origin.
  SiteInstanceGroupId site_instance_group_id_;

  // A boolean to hold the accessibility value retrieved from the `DropData`.
  // See https://crbug.com/1264873.
  bool image_accessible_from_frame_ = true;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_DRAG_SECURITY_INFO_H_
