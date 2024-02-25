// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_WEB_CONTENTS_VIEW_COCOA_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_WEB_CONTENTS_VIEW_COCOA_H_

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/common/web_contents_ns_view_bridge.mojom.h"
#import "ui/base/cocoa/base_view.h"
#import "ui/base/cocoa/views_hostable.h"

namespace content {
struct DropData;
}  // namespace content

namespace remote_cocoa::mojom {
class WebContentsNSViewHost;
}  // namespace remote_cocoa::mojom

namespace url {
class Origin;
}

@class WebDragSource;

CONTENT_EXPORT
@interface WebContentsViewCocoa
    : BaseView <ViewsHostable, NSDraggingSource, NSDraggingDestination>

// Set or un-set the mojo interface through which to communicate with the
// browser process.
- (void)setHost:(remote_cocoa::mojom::WebContentsNSViewHost*)host;

- (void)setMouseDownCanMoveWindow:(BOOL)canMove;

// Enable the workaround for https://crbug.com/1148078. This is called by
// in-PWA-process instances, to limit the workaround's effect to just PWAs.
- (void)enableDroppedScreenShotCopier;

// Private interface.
// TODO(ccameron): Document these functions.
- (instancetype)initWithViewsHostableView:(ui::ViewsHostableView*)v;
- (void)registerDragTypes;
- (void)startDragWithDropData:(const content::DropData&)dropData
                 sourceOrigin:(const url::Origin&)sourceOrigin
            dragOperationMask:(NSDragOperation)operationMask
                        image:(NSImage*)image
                       offset:(NSPoint)offset
                 isPrivileged:(BOOL)isPrivileged;
- (void)clearViewsHostableView;
- (void)viewDidBecomeFirstResponder:(NSNotification*)notification;

// API exposed for testing.

// Used to set the web contents's visibility status to occluded after a delay.
- (void)performDelayedSetWebContentsOccluded;

// Returns YES if the WCVC is scheduled to set its web contents's to the
// occluded state.
- (BOOL)willSetWebContentsOccludedAfterDelayForTesting;

// Updates the WCVC's web contents's visibility state. The update may occur
// immediately or in the near future.
- (void)updateWebContentsVisibility:(remote_cocoa::mojom::Visibility)visibility;

- (void)updateWindowControlsOverlay:(const gfx::Rect&)boundingRect;

@end

@interface NSWindow (WebContentsViewCocoa)
// Returns all the WebContentsViewCocoas in the window.
- (NSArray<WebContentsViewCocoa*>*)webContentsViewCocoa;
// Returns YES if the window contains at least one WebContentsViewCocoa.
- (BOOL)containsWebContentsViewCocoa;
@end

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_WEB_CONTENTS_VIEW_COCOA_H_
