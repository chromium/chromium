// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_SHARING_SERVICE_PICKER_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_SHARING_SERVICE_PICKER_H_

#include <AppKit/AppKit.h>

#include "content/common/render_widget_host_ns_view.mojom.h"

@interface SharingServicePicker
    : NSObject <NSSharingServiceDelegate, NSSharingServicePickerDelegate>
// Displays the NSSharingServicePicker which is positioned next to the mouse
// cursor.
- (instancetype)initWithItems:(NSArray*)items
                     callback:(remote_cocoa::mojom::RenderWidgetHostNSView::
                                   ShowSharingServicePickerCallback)cb
                         view:(NSView*)view;
- (void)show;
@end

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_SHARING_SERVICE_PICKER_H_
