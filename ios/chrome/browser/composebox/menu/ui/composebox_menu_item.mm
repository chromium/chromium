// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item.h"

@implementation ComposeboxMenuItem

- (instancetype)initWithTitle:(NSString*)title
                        image:(UIImage*)image
                         type:(ComposeboxMenuItemType)type
                     disabled:(BOOL)disabled
                      favicon:(UIImage*)favicon {
  self = [super init];
  if (self) {
    _title = [title copy];
    _image = image;
    _type = type;
    _disabled = disabled;
    _favicon = favicon;
  }
  return self;
}

- (instancetype)initWithTitle:(NSString*)title
                        image:(UIImage*)image
                         type:(ComposeboxMenuItemType)type
                     disabled:(BOOL)disabled {
  return [self initWithTitle:title
                       image:image
                        type:type
                    disabled:disabled
                     favicon:nil];
}

- (instancetype)initWithTitle:(NSString*)title
                        image:(UIImage*)image
                         type:(ComposeboxMenuItemType)type {
  return [self initWithTitle:title
                       image:image
                        type:type
                    disabled:NO
                     favicon:nil];
}

- (BOOL)isAttachmentType {
  return self.type == ComposeboxMenuItemType::kCurrentTab ||
         self.type == ComposeboxMenuItemType::kAttachmentTabs ||
         self.type == ComposeboxMenuItemType::kAttachmentCamera ||
         self.type == ComposeboxMenuItemType::kAttachmentGallery ||
         self.type == ComposeboxMenuItemType::kAttachmentFiles;
}

@end
