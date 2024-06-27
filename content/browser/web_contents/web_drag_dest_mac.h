// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_DEST_MAC_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_DEST_MAC_H_

#import <Cocoa/Cocoa.h>

#include "content/browser/web_contents/web_contents_view_drag_security_info.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/common/drop_data.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/gfx/geometry/point_f.h"

namespace content {
class RenderViewHost;
class RenderWidgetHostImpl;
class WebContentsImpl;
class WebDragDestDelegate;
}  // namespace content

// A typedef for a RenderViewHost used for comparison purposes only.
using RenderViewHostIdentifier = content::RenderViewHost*;

namespace remote_cocoa::mojom {
class DraggingInfo;
}  // namespace remote_cocoa::mojom

namespace content {
class WebContentsViewDelegate;

// A structure used to keep drop context for asynchronously finishing a drop
// operation. This is required because some drop event data can change before
// completeDropAsync is called.
struct DropContext {
  DropContext(const DropData drop_data,
              const gfx::PointF client_pt,
              const gfx::PointF screen_pt,
              int modifier_flags,
              base::WeakPtr<RenderWidgetHostImpl> target_rwh);
  DropContext(const DropContext& other);
  DropContext(DropContext&& other);
  ~DropContext();

  DropData drop_data;
  const gfx::PointF client_pt;
  const gfx::PointF screen_pt;
  const int modifier_flags;
  base::WeakPtr<RenderWidgetHostImpl> target_rwh;
};

// Given a pasteboard, returns a `DropData` filled using its contents. The types
// handled by this method must be kept in sync with `-[WebContentsViewCocoa
// registerDragTypes]`.
DropData CONTENT_EXPORT PopulateDropDataFromPasteboard(NSPasteboard* pboard);

}  // namespace content

// A class that handles tracking and event processing for a drag and drop
// over the content area. Assumes something else initiates the drag, this is
// only for processing during a drag.
CONTENT_EXPORT
@interface WebDragDest : NSObject

// |contents| is the WebContentsImpl representing this tab, used to communicate
// drag&drop messages to WebCore and handle navigation on a successful drop
// (if necessary).
- (id)initWithWebContentsImpl:(content::WebContentsImpl*)contents;

- (content::DropData*)currentDropData;

- (void)setDragDelegate:(content::WebDragDestDelegate*)delegate;

// Sets the current operation negotiated by the source and destination,
// which determines whether or not we should allow the drop. Takes effect the
// next time |-draggingUpdated:| is called.
//
// See mojo method FrameWidget::DragTargetDragEnter() for a discussion of
// |operation| and |documentOperation|.
- (void)setCurrentOperation:(ui::mojom::DragOperation)operation
     documentIsHandlingDrag:(bool)documentIsHandlingDrag;

// Messages to send during the tracking of a drag, usually upon receiving
// calls from the view system. Communicates the drag messages to WebCore.
- (void)setDropData:(const content::DropData&)dropData;
- (NSDragOperation)draggingEntered:
    (const remote_cocoa::mojom::DraggingInfo*)info;
- (void)draggingExited;
- (NSDragOperation)draggingUpdated:
    (const remote_cocoa::mojom::DraggingInfo*)info;
- (BOOL)performDragOperation:(const remote_cocoa::mojom::DraggingInfo*)info
    withWebContentsViewDelegate:
        (content::WebContentsViewDelegate*)webContentsViewDelegate;
- (void)completeDropAsync:(std::optional<content::DropData>)dropData
              withContext:(const content::DropContext)context;

// Helper to call WebWidgetHostInputEventRouter::GetRenderWidgetHostAtPoint().
- (content::RenderWidgetHostImpl*)
    GetRenderWidgetHostAtPoint:(const gfx::PointF&)viewPoint
                 transformedPt:(gfx::PointF*)transformedPt;

// Called to indicate that the owning WebContents has initiated a drag.
- (void)initiateDragWithRenderWidgetHost:(content::RenderWidgetHostImpl*)rwhi
                                dropData:(const content::DropData&)dropData;

// Called to indicate that, if the owning WebContents has initiated a drag, that
// drag has ended.
- (void)endDrag;

@end

// Public use only for unit tests.
@interface WebDragDest (Testing)
// Given a point in window coordinates and a view in that window, return a
// flipped point in the coordinate system of |view|.
- (NSPoint)flipWindowPointToView:(const NSPoint&)windowPoint
                            view:(NSView*)view;
// Given a point in window coordinates and a view in that window, return a
// flipped point in screen coordinates.
- (NSPoint)flipWindowPointToScreen:(const NSPoint&)windowPoint
                              view:(NSView*)view;
@end

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_DRAG_DEST_MAC_H_
