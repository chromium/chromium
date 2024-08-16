// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activities/request_desktop_or_mobile_site_activity.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

NSString* const kRequestDesktopOrMobileSiteActivityType =
    @"com.google.chrome.requestDesktopOrMobileSiteActivity";

}  // namespace

@interface RequestDesktopOrMobileSiteActivity ()

// User agent type of the current page.
@property(nonatomic, assign) web::UserAgentType userAgent;
// The handler that is invoked when the IPH bubble is displayed.
@property(nonatomic, weak) id<HelpCommands> helpHandler;
// The agent that is invoked when the activity is performed.
@property(nonatomic, readonly) WebNavigationBrowserAgent* agent;

@end

@implementation RequestDesktopOrMobileSiteActivity

- (instancetype)initWithUserAgent:(web::UserAgentType)userAgent
                      helpHandler:(id<HelpCommands>)helpHandler
                  navigationAgent:(WebNavigationBrowserAgent*)agent {
  self = [super init];
  if (self) {
    _userAgent = userAgent;
    _helpHandler = helpHandler;
    _agent = agent;
  }
  return self;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return kRequestDesktopOrMobileSiteActivityType;
}

- (NSString*)activityTitle {
  if (self.userAgent == web::UserAgentType::MOBILE) {
    return l10n_util::GetNSString(IDS_IOS_SHARE_MENU_REQUEST_DESKTOP_SITE);
  }
  return l10n_util::GetNSString(IDS_IOS_SHARE_MENU_REQUEST_MOBILE_SITE);
}

- (UIImage*)activityImage {
  NSString* symbolName = self.userAgent == web::UserAgentType::MOBILE
                             ? kDesktopSymbol
                             : kIPhoneSymbol;
  return MakeSymbolMonochrome(
      DefaultSymbolWithPointSize(symbolName, kSymbolActionPointSize));
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return self.userAgent != web::UserAgentType::NONE;
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  [self activityDidFinish:YES];
  if (self.userAgent == web::UserAgentType::MOBILE) {
    base::RecordAction(
        base::UserMetricsAction("MobileShareActionRequestDesktop"));
    self.agent->RequestDesktopSite();
    [self.helpHandler
        presentInProductHelpWithType:InProductHelpType::kDefaultSiteView];
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileShareActionRequestMobile"));
    self.agent->RequestMobileSite();
  }
}

@end
