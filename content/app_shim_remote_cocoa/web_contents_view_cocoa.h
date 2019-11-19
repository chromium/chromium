// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_WEB_CONTENTS_VIEW_COCOA_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_WEB_CONTENTS_VIEW_COCOA_H_

#include "base/mac/scoped_nsobject.h"
#include "content/common/content_export.h"
#import "ui/base/cocoa/base_view.h"
#import "ui/base/cocoa/views_hostable.h"

namespace content {
struct DropData;
}  // namespace content

namespace remote_cocoa {
namespace mojom {
class WebContentsNSViewHost;
}  // namespace mojom
}  // namespace remote_cocoa

@class WebDragSource;

CONTENT_EXPORT
@interface WebContentsViewCocoa : BaseView <ViewsHostable> {
 @private
  // Instances of this class are owned by both host_ and AppKit. It is
  // possible for an instance to outlive its webContentsView_. The host_ must
  // call -clearHostAndView in its destructor.
  remote_cocoa::mojom::WebContentsNSViewHost* host_;

  // The interface exported to views::Views that embed this as a sub-view.
  ui::ViewsHostableView* viewsHostableView_;

  base::scoped_nsobject<WebDragSource> dragSource_;
  BOOL mouseDownCanMoveWindow_;
}

// Set or un-set the mojo interface through which to communicate with the
// browser process.
- (void)setHost:(remote_cocoa::mojom::WebContentsNSViewHost*)host;

- (void)setMouseDownCanMoveWindow:(BOOL)canMove;

// Returns the available drag operations. This is a required method for
// NSDraggingSource. It is supposedly deprecated, but the non-deprecated API
// -[NSWindow dragImage:...] still relies on it.
- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal;

// Private interface.
// TODO(ccameron): Document these functions.
- (id)initWithViewsHostableView:(ui::ViewsHostableView*)v;
- (void)registerDragTypes;
- (void)startDragWithDropData:(const content::DropData&)dropData
            dragOperationMask:(NSDragOperation)operationMask
                        image:(NSImage*)image
                       offset:(NSPoint)offset;
- (void)clearViewsHostableView;
- (void)updateWebContentsVisibility;
- (void)viewDidBecomeFirstResponder:(NSNotification*)notification;
@end

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_WEB_CONTENTS_VIEW_COCOA_H_
