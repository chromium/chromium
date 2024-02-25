// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app_shim_remote_cocoa/sharing_service_picker.h"

#include <string>
#include <utility>

#include "base/strings/sys_string_conversions.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#include "url/gurl.h"

@implementation SharingServicePicker {
  NSSharingServicePicker* __strong picker_;
  remote_cocoa::mojom::RenderWidgetHostNSView::ShowSharingServicePickerCallback
      callback_;
  NSView* __strong view_;
}

- (instancetype)initWithItems:(NSArray*)items
                     callback:(remote_cocoa::mojom::RenderWidgetHostNSView::
                                   ShowSharingServicePickerCallback)cb
                         view:(NSView*)view {
  if ((self = [super init])) {
    picker_ = [[NSSharingServicePicker alloc] initWithItems:items];
    picker_.delegate = self;
    callback_ = std::move(cb);
    view_ = view;
  }
  return self;
}

- (void)sharingServicePicker:(NSSharingServicePicker*)sharingServicePicker
     didChooseSharingService:(NSSharingService*)service {
  // When the NSSharingServicePicker gets invoked but then the picker gets
  // dismissed, this is the only delegate method called, and it's called with a
  // nil service, so run the callback.
  if (!service) {
    std::move(callback_).Run(blink::mojom::ShareError::CANCELED);
  }
}

- (void)show {
  NSPoint location = [view_.window mouseLocationOutsideOfEventStream];
  NSRect rect = NSMakeRect(location.x, location.y, 1.0, 1.0);
  [picker_ showRelativeToRect:rect ofView:view_ preferredEdge:NSMinYEdge];
}

- (void)sharingService:(NSSharingService*)sharingService
         didShareItems:(NSArray*)items {
  std::move(callback_).Run(blink::mojom::ShareError::OK);
}

- (void)sharingService:(NSSharingService*)sharingService
    didFailToShareItems:(NSArray*)items
                  error:(NSError*)error {
  error.code == NSUserCancelledError
      ? std::move(callback_).Run(blink::mojom::ShareError::CANCELED)
      : std::move(callback_).Run(blink::mojom::ShareError::INTERNAL_ERROR);
}

- (id<NSSharingServiceDelegate>)
         sharingServicePicker:(NSSharingServicePicker*)sharingServicePicker
    delegateForSharingService:(NSSharingService*)sharingService {
  return self;
}

- (NSWindow*)sharingService:(NSSharingService*)sharingService
    sourceWindowForShareItems:(NSArray*)items
          sharingContentScope:(NSSharingContentScope*)sharingContentScope {
  return view_.window;
}

@end
