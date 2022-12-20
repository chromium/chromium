// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_WEB_DRAG_SOURCE_MAC_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_WEB_DRAG_SOURCE_MAC_H_

#include "base/memory/raw_ptr.h"

#import <Cocoa/Cocoa.h>

#include "base/files/file_path.h"
#include "base/mac/scoped_nsobject.h"
#include "content/common/content_export.h"
#include "content/public/common/drop_data.h"
#include "url/gurl.h"

namespace content {
struct DropData;
}  // namespace content

namespace remote_cocoa::mojom {
class WebContentsNSViewHost;
}  // namespace remote_cocoa::mojom

// A class that handles managing the data for drags from the
// WebContentsViewCocoa.
CONTENT_EXPORT
@interface WebDragSource : NSObject <NSPasteboardWriting> {
 @private
  // The host through which to communicate with the WebContents. Owns
  // this object. This pointer gets reset when the WebContents goes away with
  // `webContentsIsGone`.
  raw_ptr<remote_cocoa::mojom::WebContentsNSViewHost> _host;

  // The drop data.
  content::DropData _dropData;

  // The file name to be saved to for a drag-out download.
  base::FilePath _downloadFileName;

  // The URL to download from for a drag-out download.
  GURL _downloadURL;

  // The file type associated with the file drag, if any. TODO(macOS 11): Change
  // to a UTType object.
  base::scoped_nsobject<NSString> _fileUTType;
}

// Initialize a WebDragSource object for a drag.
- (instancetype)initWithHost:(remote_cocoa::mojom::WebContentsNSViewHost*)host
                    dropData:(const content::DropData&)dropData;

// Call when the WebContents is gone.
- (void)webContentsIsGone;

@end

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_WEB_DRAG_SOURCE_MAC_H_
