// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_image_source.h"

#import <LinkPresentation/LinkPresentation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/check.h"

@interface ChromeActivityImageSource ()

// The shared image.
@property(nonatomic, strong) UIImage* image;

// The image's title.
@property(nonatomic, strong) NSString* title;

@end

@implementation ChromeActivityImageSource

- (instancetype)initWithImage:(UIImage*)image title:(NSString*)title {
  DCHECK(image);
  DCHECK(title);
  if ((self = [super init])) {
    _image = image;
    _title = title;
  }
  return self;
}

#pragma mark - ChromeActivityItemSource

- (NSSet*)excludedActivityTypes {
  return [NSSet
      setWithArray:@[ UIActivityTypeAssignToContact, UIActivityTypePrint ]];
}

#pragma mark - UIActivityItemSource

- (id)activityViewController:(UIActivityViewController*)activityViewController
         itemForActivityType:(NSString*)activityType {
  return self.image;
}

- (id)activityViewControllerPlaceholderItem:
    (UIActivityViewController*)activityViewController {
  return self.image;
}

- (NSString*)activityViewController:
                 (UIActivityViewController*)activityViewController
    dataTypeIdentifierForActivityType:(UIActivityType)activityType {
  return UTTypeImage.identifier;
}

- (LPLinkMetadata*)activityViewControllerLinkMetadata:
    (UIActivityViewController*)activityViewController {
  NSItemProvider* imageProvider =
      [[NSItemProvider alloc] initWithObject:self.image];

  LPLinkMetadata* metadata = [[LPLinkMetadata alloc] init];
  metadata.imageProvider = imageProvider;
  metadata.title = self.title;

  return metadata;
}

@end
