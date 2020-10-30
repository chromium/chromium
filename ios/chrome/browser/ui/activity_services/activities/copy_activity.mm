// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/copy_activity.h"

#import "ios/chrome/browser/ui/activity_services/data/share_to_data.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kCopyActivityType = @"com.google.chrome.copyActivity";

}  // namespace

@interface CopyActivity ()

@property(nonatomic, strong) ShareToData* data;

@end

@implementation CopyActivity

#pragma mark - Public

- (instancetype)initWithData:(ShareToData*)data {
  DCHECK(data);
  self = [super init];
  if (self) {
    _data = data;
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
  return [UIImage imageNamed:@"activity_services_copy"];
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return !!self.data;
}

- (void)prepareWithActivityItems:(NSArray*)activityItems {
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  if (self.data.additionalText) {
    StoreInPasteboard(self.data.additionalText, self.data.shareURL);
  } else {
    StoreURLInPasteboard(self.data.shareURL);
  }
  [self activityDidFinish:YES];
}

@end
