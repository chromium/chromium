// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activities/find_in_page_activity.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

NSString* const kFindInPageActivityType =
    @"com.google.chrome.FindInPageActivityType";

}  // namespace

@interface FindInPageActivity ()
// Data associated with this activity.
@property(nonatomic, strong, readonly) ShareToData* data;
// The handler that handles the command when the activity is performed.
@property(nonatomic, weak, readonly) id<FindInPageCommands> handler;
@end

@implementation FindInPageActivity

- (instancetype)initWithData:(ShareToData*)data
                     handler:(id<FindInPageCommands>)handler {
  self = [super init];
  if (self) {
    _data = data;
    _handler = handler;
  }
  return self;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return kFindInPageActivityType;
}

- (NSString*)activityTitle {
  return l10n_util::GetNSString(IDS_IOS_SHARE_MENU_FIND_IN_PAGE);
}

- (UIImage*)activityImage {
  return DefaultSymbolWithPointSize(kFindInPageActionSymbol,
                                    kSymbolActionPointSize);
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return self.data.isPageSearchable;
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  [self activityDidFinish:YES];
  [self.handler openFindInPage];
}

@end
