// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activities/copy_activity.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

NSString* const kCopyActivityType = @"com.google.chrome.copyActivity";

}  // namespace

@interface CopyActivity ()

@property(nonatomic, strong) NSArray<ShareToData*>* dataItems;

@end

@implementation CopyActivity

#pragma mark - Public

- (instancetype)initWithDataItems:(NSArray<ShareToData*>*)dataItems {
  DCHECK(dataItems);
  DCHECK(dataItems.count);
  self = [super init];
  if (self) {
    _dataItems = dataItems;
  }
  return self;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return kCopyActivityType;
}

- (NSString*)activityTitle {
  return l10n_util::GetNSString(IDS_IOS_SHARE_MENU_COPY);
}

- (UIImage*)activityImage {
  return DefaultSymbolWithPointSize(kCopyActionSymbol, kSymbolActionPointSize);
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return !!self.dataItems && self.dataItems.count;
}

- (void)prepareWithActivityItems:(NSArray*)activityItems {
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  [self activityDidFinish:YES];
  if (self.dataItems.count == 1 && self.dataItems.firstObject.additionalText) {
    StoreInPasteboard(self.dataItems.firstObject.additionalText,
                      self.dataItems.firstObject.shareURL);
  } else {
    std::vector<GURL> urls;
    for (ShareToData* shareToData in self.dataItems) {
      urls.push_back(shareToData.shareURL);
    }
    StoreURLsInPasteboard(urls);
  }
}

@end
