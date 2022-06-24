// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_WEB_DRAG_SOURCE_MAC_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_WEB_DRAG_SOURCE_MAC_H_

#include "base/memory/raw_ptr.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {
struct DropData;
}  // namespace content

namespace remote_cocoa {
namespace mojom {
class WebContentsNSViewHost;
}  // namespace mojom
}  // namespace remote_cocoa

// A class that handles tracking and event processing for a drag and drop
// originating from the content area.
CONTENT_EXPORT
@interface WebDragSource : NSObject {
 @private
  // The host through which to communicate with the WebContentsImpl. Owns
  // |self| and resets |host_| via clearHostAndWebContentsView.
  raw_ptr<remote_cocoa::mojom::WebContentsNSViewHost> _host;

  // The view from which the drag was initiated. Weak reference.
  // An instance of this class may outlive |contentsView_|. The destructor of
  // |contentsView_| must set this ivar to |nullptr|.
  NSView* _contentsView;

  // Our drop data. Should only be initialized once.
  std::unique_ptr<content::DropData> _dropData;

  // The image to show as drag image. Can be nil.
  base::scoped_nsobject<NSImage> _dragImage;

  // The offset to draw |dragImage_| at.
  NSPoint _imageOffset;

  // Our pasteboard.
  base::scoped_nsobject<NSPasteboard> _pasteboard;

  // Change count associated with this pasteboard owner change.
  int _changeCount;

  // A mask of the allowed drag operations.
  NSDragOperation _dragOperationMask;

  // The file name to be saved to for a drag-out download.
  base::FilePath _downloadFileName;

  // The URL to download from for a drag-out download.
  GURL _downloadURL;

  // The file UTI associated with the file drag, if any.
  base::ScopedCFTypeRef<CFStringRef> _fileUTI;
}

// Initialize a WebDragSource object for a drag (originating on the given
// contentsView and with the given dropData and pboard). Fill the pasteboard
// with data types appropriate for dropData.
- (instancetype)initWithHost:(remote_cocoa::mojom::WebContentsNSViewHost*)host
                        view:(NSView*)contentsView
                    dropData:(const content::DropData*)dropData
                       image:(NSImage*)image
                      offset:(NSPoint)offset
                  pasteboard:(NSPasteboard*)pboard
           dragOperationMask:(NSDragOperation)dragOperationMask;

// Call when the web contents is gone.
- (void)clearHostAndWebContentsView;

// Returns a mask of the allowed drag operations.
- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal;

// Start the drag (on the originally provided contentsView); can do this right
// after -initWithContentsView:....
- (void)startDrag;

// End the drag and clear the pasteboard; hook up to
// -draggedImage:endedAt:operation:.
- (void)endDragAt:(NSPoint)screenPoint
        operation:(NSDragOperation)operation;

// Remove this WebDragSource as the owner of the drag pasteboard.
- (void)clearPasteboard;

// Call to drag a promised file to the given path (should be called before
// -endDragAt:...); hook up to -namesOfPromisedFilesDroppedAtDestination:.
// Returns the file name (not including path) of the file deposited (or which
// will be deposited).
- (NSString*)dragPromisedFileTo:(NSString*)path;

@end

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_WEB_DRAG_SOURCE_MAC_H_
