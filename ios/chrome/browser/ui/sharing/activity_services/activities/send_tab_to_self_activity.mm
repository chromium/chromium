// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activities/send_tab_to_self_activity.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

NSString* const kSendTabToSelfActivityType =
    @"com.google.chrome.sendTabToSelfActivity";

}  // namespace

@interface SendTabToSelfActivity ()
// The data object targeted by this activity.
@property(nonatomic, strong, readonly) ShareToData* data;
// The handler to be invoked when the activity is performed.
@property(nonatomic, weak, readonly) id<BrowserCoordinatorCommands> handler;

@end

@implementation SendTabToSelfActivity

- (instancetype)initWithData:(ShareToData*)data
                     handler:(id<BrowserCoordinatorCommands>)handler {
  if ((self = [super init])) {
    _data = data;
    _handler = handler;
  }
  return self;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return kSendTabToSelfActivityType;
}

- (NSString*)activityTitle {
  return l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
}

- (UIImage*)activityImage {
  return CustomSymbolWithPointSize(kRecentTabsSymbol, kSymbolActionPointSize);
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return self.data.canSendTabToSelf;
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  [self activityDidFinish:YES];
  [self.handler showSendTabToSelfUI:self.data.shareURL title:self.data.title];
}

@end
