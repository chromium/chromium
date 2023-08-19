// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_url_source.h"

#import <LinkPresentation/LinkPresentation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/check.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_item_thumbnail_generator.h"

@interface ChromeActivityURLSource () {
  NSString* _subject;
}

// URL to be shared with share extensions.
@property(nonatomic, strong) NSURL* shareURL;

@end

@implementation ChromeActivityURLSource

- (instancetype)initWithShareURL:(NSURL*)shareURL subject:(NSString*)subject {
  DCHECK(shareURL);
  DCHECK(subject);
  self = [super init];
  if (self) {
    _shareURL = shareURL;
    _subject = [subject copy];
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
  // Return the current URL as a placeholder
  return self.shareURL;
}

- (NSString*)activityViewController:
                 (UIActivityViewController*)activityViewController
             subjectForActivityType:(NSString*)activityType {
  return _subject;
}

- (id)activityViewController:(UIActivityViewController*)activityViewController
         itemForActivityType:(NSString*)activityType {
  return self.shareURL;
}

- (NSString*)activityViewController:
                 (UIActivityViewController*)activityViewController
    dataTypeIdentifierForActivityType:(NSString*)activityType {
  return UTTypeURL.identifier;
}

- (UIImage*)activityViewController:
                (UIActivityViewController*)activityViewController
     thumbnailImageForActivityType:(UIActivityType)activityType
                     suggestedSize:(CGSize)size {
  return [self.thumbnailGenerator thumbnailWithSize:size];
}

- (LPLinkMetadata*)activityViewControllerLinkMetadata:
    (UIActivityViewController*)activityViewController {
  return _linkMetadata;
}

@end
