// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_WEB_DRAG_SOURCE_MAC_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_WEB_DRAG_SOURCE_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/public/common/drop_data.h"
#include "url/gurl.h"

namespace content {
struct DropData;
}  // namespace content

namespace remote_cocoa::mojom {
class WebContentsNSViewHost;
}  // namespace remote_cocoa::mojom

namespace url {
class Origin;
}

// A class that handles managing the data for drags from the
// WebContentsViewCocoa.
CONTENT_EXPORT
@interface WebDragSource : NSObject <NSPasteboardWriting>

// Initialize a WebDragSource object for a drag.
- (instancetype)initWithHost:(remote_cocoa::mojom::WebContentsNSViewHost*)host
                    dropData:(const content::DropData&)dropData
                sourceOrigin:(const url::Origin&)sourceOrigin
                isPrivileged:(BOOL)privileged;

// Call when the WebContents is gone.
- (void)webContentsIsGone;

@end

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_WEB_DRAG_SOURCE_MAC_H_
