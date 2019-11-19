// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/chrome_activity_item_source.h"

#import <MobileCoreServices/MobileCoreServices.h>

#include "base/logging.h"
#import "ios/chrome/browser/ui/activity_services/activity_type_util.h"
#import "ios/chrome/browser/ui/activity_services/appex_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - UIActivityImageSource

@interface UIActivityImageSource () {
  // The shared image.
  UIImage* _image;
}

@end

@implementation UIActivityImageSource

- (instancetype)initWithImage:(UIImage*)image {
  DCHECK(image);
  self = [super init];
  if (self) {
    _image = image;
  }
  return self;
}

#pragma mark - UIActivityItemSource

- (id)activityViewController:(UIActivityViewController*)activityViewController
         itemForActivityType:(NSString*)activityType {
  return _image;
}

- (id)activityViewControllerPlaceholderItem:
    (UIActivityViewController*)activityViewController {
  return _image;
}

@end

#pragma mark - UIActivityURLSource

@interface UIActivityURLSource () {
  NSString* _subject;
  ChromeActivityItemThumbnailGenerator* _thumbnailGenerator;
}

// URL to be shared with share extensions.
@property(nonatomic, strong) NSURL* shareURL;
// URL to be shared with password managers.
@property(nonatomic, strong) NSURL* passwordManagerURL;

@end

@implementation UIActivityURLSource

@synthesize shareURL = _shareURL;
@synthesize passwordManagerURL = _passwordManagerURL;

- (instancetype)initWithShareURL:(NSURL*)shareURL
              passwordManagerURL:(NSURL*)passwordManagerURL
                         subject:(NSString*)subject
              thumbnailGenerator:
                  (ChromeActivityItemThumbnailGenerator*)thumbnailGenerator {
  DCHECK(shareURL);
  DCHECK(passwordManagerURL);
  DCHECK(subject);
  DCHECK(thumbnailGenerator);
  self = [super init];
  if (self) {
    _shareURL = shareURL;
    _passwordManagerURL = passwordManagerURL;
    _subject = [subject copy];
    _thumbnailGenerator = thumbnailGenerator;
  }
  return self;
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
  if (activity_type_util::TypeFromString(activityType) !=
      activity_type_util::APPEX_PASSWORD_MANAGEMENT)
    return self.shareURL;

  // Constructs an NSExtensionItem object from the URL designated for password
  // managers.
  NSDictionary* appExItems = @{
    activity_services::kPasswordAppExVersionNumberKey :
        activity_services::kPasswordAppExVersionNumber,
    activity_services::
    kPasswordAppExURLStringKey : [self.passwordManagerURL absoluteString]
  };
  NSItemProvider* itemProvider = [[NSItemProvider alloc]
        initWithItem:appExItems
      typeIdentifier:activity_services::kUTTypeAppExtensionFindLoginAction];
  NSExtensionItem* extensionItem = [[NSExtensionItem alloc] init];
  [extensionItem setAttachments:@[ itemProvider ]];
  return extensionItem;
}

- (NSString*)activityViewController:
                 (UIActivityViewController*)activityViewController
    dataTypeIdentifierForActivityType:(NSString*)activityType {
  // This UTI now satisfies both the Password Management App Extension and the
  // usual NSURL for Share extensions. If the following DCHECK fails, it is
  // probably due to missing or incorrect UTImportedTypeDeclarations in
  // Info.plist.
  NSString* findLoginType =
      activity_services::kUTTypeAppExtensionFindLoginAction;
  DCHECK(UTTypeConformsTo((__bridge CFStringRef)findLoginType, kUTTypeURL));
  DCHECK(UTTypeConformsTo((__bridge CFStringRef)findLoginType,
                          (__bridge CFStringRef)
                              @"org.appextension.chrome-password-action"));
  // This method may be called before or after the presentation of the
  // UIActivityViewController. When called before (i.e. user has not made a
  // selection of which AppEx to launch), |activityType| is nil. If and when
  // called after UIActivityViewController has been presented and dismissed
  // after user made a choice of which AppEx to run, this method may be called
  // with |activityType| equals to the bundle ID of the AppEx selected.
  // Default action is to return @"public.url" UTType.
  if (!activityType || activity_type_util::TypeFromString(activityType) ==
                           activity_type_util::APPEX_PASSWORD_MANAGEMENT)
    return findLoginType;
  return (NSString*)kUTTypeURL;
}

- (UIImage*)activityViewController:
                (UIActivityViewController*)activityViewController
     thumbnailImageForActivityType:(UIActivityType)activityType
                     suggestedSize:(CGSize)size {
  return [_thumbnailGenerator thumbnailWithSize:size];
}

@end
