// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_text_source.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/check.h"

@interface ChromeActivityTextSource ()

// Text to be shared with share extensions.
@property(nonatomic, strong) NSString* text;

@end

@implementation ChromeActivityTextSource

- (instancetype)initWithText:(NSString*)text {
  DCHECK(text);
  if ((self = [super init])) {
    _text = [text copy];
  }
  return self;
}

#pragma mark - ChromeActivityItemSource

- (NSSet*)excludedActivityTypes {
  return [NSSet setWithArray:@[
    UIActivityTypeAddToReadingList, UIActivityTypeCopyToPasteboard,
    UIActivityTypePrint, UIActivityTypeSaveToCameraRoll
  ]];
}

#pragma mark - UIActivityItemSource

- (id)activityViewControllerPlaceholderItem:
    (UIActivityViewController*)activityViewController {
  return self.text;
}

- (id)activityViewController:(UIActivityViewController*)activityViewController
         itemForActivityType:(NSString*)activityType {
  return self.text;
}

- (NSString*)activityViewController:
                 (UIActivityViewController*)activityViewController
    dataTypeIdentifierForActivityType:(NSString*)activityType {
  return UTTypeText.identifier;
}

@end
