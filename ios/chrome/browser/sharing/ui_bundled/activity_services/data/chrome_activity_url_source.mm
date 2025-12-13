// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/chrome_activity_url_source.h"

#import <LinkPresentation/LinkPresentation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/check.h"
#import "base/feature_list.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/chrome_activity_item_thumbnail_generator.h"

namespace {
// Feature flag to restore sharing just the data instead of an Extension Item.
// To be used as a kill switch.
BASE_FEATURE(kShareNSExtensionItemKillSwitch,
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace

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
  if (base::FeatureList::IsEnabled(kShareNSExtensionItemKillSwitch)) {
    return _shareURL;
  }
  if ([activityType isEqualToString:UIActivityTypeMessage] ||
      [activityType isEqualToString:UIActivityTypeMail]) {
    // Message and mail do not seem to support NSItemProvider.
    return _shareURL;
  }
  NSItemProvider* provider =
      [[NSItemProvider alloc] initWithItem:_shareURL
                            typeIdentifier:UTTypeURL.identifier];
  __weak __typeof(self) weakSelf = self;
  provider.previewImageHandler = ^(
      NSItemProviderCompletionHandler completionHandler,
      Class expectedValueClass, NSDictionary* options) {
    CGSize preferredImageSize = [[options
        objectForKey:NSItemProviderPreferredImageSizeKey] CGSizeValue];
    if (CGSizeEqualToSize(preferredImageSize, CGSizeZero)) {
      preferredImageSize = CGSizeMake(256, 256);
    }
    completionHandler([weakSelf activityViewController:activityViewController
                          thumbnailImageForActivityType:activityType
                                          suggestedSize:preferredImageSize],
                      nil);
  };
  NSExtensionItem* item = [[NSExtensionItem alloc] init];
  item.attachments = @[ provider ];
  if (_subject) {
    item.attributedTitle = [[NSAttributedString alloc] initWithString:_subject];
  }
  return item;
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
