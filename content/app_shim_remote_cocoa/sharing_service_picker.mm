// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app_shim_remote_cocoa/sharing_service_picker.h"

#include <utility>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#include "url/gurl.h"

@interface SharingServicePicker
    : NSObject <NSSharingServiceDelegate, NSSharingServicePickerDelegate>
// Displays the NSSharingServicePicker which is positioned center and overlaps
// WebContents and the Non Client area.
- (void)show;
@end

@implementation SharingServicePicker {
  base::scoped_nsobject<NSSharingServicePicker> picker_;
  remote_cocoa::mojom::RenderWidgetHostNSView::ShowSharingServicePickerCallback
      callback_;
  NSView* view_;
}

- (instancetype)initWithItems:(NSArray*)items
                     callback:(remote_cocoa::mojom::RenderWidgetHostNSView::
                                   ShowSharingServicePickerCallback)cb
                         view:(NSView*)view {
  if ((self = [super init])) {
    picker_.reset([[NSSharingServicePicker alloc] initWithItems:items]);
    picker_.get().delegate = self;
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
  NSRect viewFrame = [view_ frame];
  CGSize size = viewFrame.size;
  NSRect rect = NSMakeRect(size.width / 2, size.height, 1, 1);
  [picker_ showRelativeToRect:rect ofView:view_ preferredEdge:NSMaxXEdge];
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
  return [view_ window];
}

@end

namespace remote_cocoa {

void ShowSharingServicePickerForView(
    NSView* view,
    const std::string& title,
    const std::string& text,
    const std::string& url,
    const std::vector<std::string>& file_paths,
    mojom::RenderWidgetHostNSView::ShowSharingServicePickerCallback callback) {
  NSString* ns_title = base::SysUTF8ToNSString(title);
  NSString* ns_url = base::SysUTF8ToNSString(url);
  NSString* ns_text = base::SysUTF8ToNSString(text);

  NSMutableArray* items =
      [NSMutableArray arrayWithArray:@[ ns_title, ns_url, ns_text ]];

  for (const auto& file_path : file_paths) {
    NSString* ns_file_path = base::SysUTF8ToNSString(file_path);
    NSURL* file_url = [NSURL fileURLWithPath:ns_file_path];
    [items addObject:file_url];
  }

  base::scoped_nsobject<SharingServicePicker> picker(
      [[SharingServicePicker alloc] initWithItems:items
                                         callback:std::move(callback)
                                             view:view]);
  [picker.get() show];
}

}  // namespace remote_cocoa
