// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/first_run/welcome_to_chrome_view_controller.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/first_run/first_run_configuration.h"
#include "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/ui/fancy_ui/primary_action_button.h"
#import "ios/chrome/browser/ui/first_run/first_run_chrome_signin_view_controller.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#include "ios/chrome/browser/ui/first_run/first_run_util.h"
#include "ios/chrome/browser/ui/first_run/static_file_view_controller.h"
#import "ios/chrome/browser/ui/first_run/welcome_to_chrome_view.h"
#include "ios/chrome/browser/ui/util/terms_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kFadeOutAnimationDuration = 0.16f;

// Default value for metrics reporting state. "YES" corresponding to "opt-out"
// state.
const BOOL kDefaultStatsCheckboxValue = YES;
}

@interface WelcomeToChromeViewController ()<WelcomeToChromeViewDelegate,
                                            UINavigationControllerDelegate> {
  Browser* _browser;
}

// The animation which occurs at launch has run.
@property(nonatomic, assign) BOOL ranLaunchAnimation;

// The TOS link was tapped.
@property(nonatomic, assign) BOOL didTapTOSLink;

// The privacy link was tapped.
@property(nonatomic, assign) BOOL didTapPrivacyLink;

// The status of the privacy link load.
@property(nonatomic, assign) MobileFreLinkTappedStatus privacyLinkStatus;

// Presenter for showing sync-related UI.
@property(nonatomic, readonly, weak) id<SyncPresenter> presenter;

@property(nonatomic, readonly, weak) id<ApplicationCommands> dispatcher;

@end

@implementation WelcomeToChromeViewController

@synthesize didTapPrivacyLink = _didTapPrivacyLink;
@synthesize didTapTOSLink = _didTapTOSLink;
@synthesize ranLaunchAnimation = _ranLaunchAnimation;
@synthesize presenter = _presenter;
@synthesize dispatcher = _dispatcher;
@synthesize privacyLinkStatus = _privacyLinkStatus;

+ (BOOL)defaultStatsCheckboxValue {
  // Record metrics reporting as opt-in/opt-out only once.
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    // Don't call RecordMetricsReportingDefaultState twice.  This can happen
    // if the app is quit before accepting the TOS, or via experiment settings.
    if (metrics::GetMetricsReportingDefaultState(
            GetApplicationContext()->GetLocalState()) !=
        metrics::EnableMetricsDefault::DEFAULT_UNKNOWN) {
      return;
    }

    metrics::RecordMetricsReportingDefaultState(
        GetApplicationContext()->GetLocalState(),
        kDefaultStatsCheckboxValue ? metrics::EnableMetricsDefault::OPT_OUT
                                   : metrics::EnableMetricsDefault::OPT_IN);
  });
  return kDefaultStatsCheckboxValue;
}

- (instancetype)initWithBrowser:(Browser*)browser
                      presenter:(id<SyncPresenter>)presenter
                     dispatcher:(id<ApplicationCommands>)dispatcher {
  DCHECK(browser);
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _browser = browser;
    _presenter = presenter;
    _dispatcher = dispatcher;
  }
  return self;
}

- (void)loadView {
  WelcomeToChromeView* welcomeToChromeView =
      [[WelcomeToChromeView alloc] initWithFrame:CGRectZero];
  [welcomeToChromeView setDelegate:self];
  [welcomeToChromeView
      setCheckBoxSelected:[[self class] defaultStatsCheckboxValue]];
  self.view = welcomeToChromeView;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self.navigationController setNavigationBarHidden:YES];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if (self.ranLaunchAnimation)
    return;
  WelcomeToChromeView* view =
      base::mac::ObjCCastStrict<WelcomeToChromeView>(self.view);
  [view runLaunchAnimation];
  self.ranLaunchAnimation = YES;
}

- (BOOL)prefersStatusBarHidden {
  return YES;
}

- (NSURL*)newTermsOfServiceUrl {
  std::string tos = GetTermsOfServicePath();
  NSString* path = [[base::mac::FrameworkBundle() bundlePath]
      stringByAppendingPathComponent:base::SysUTF8ToNSString(tos)];
  NSURLComponents* components = [[NSURLComponents alloc] init];
  [components setScheme:@"file"];
  [components setHost:@""];
  [components setPath:path];
  return [components URL];
}

// Displays the file at the given URL in a StaticFileViewController.
- (void)openStaticFileWithURL:(NSURL*)url title:(NSString*)title {
  StaticFileViewController* staticViewController =
      [[StaticFileViewController alloc]
          initWithBrowserState:_browser->GetBrowserState()
                           URL:url];
  [staticViewController setTitle:title];
  [self.navigationController pushViewController:staticViewController
                                       animated:YES];
}

#pragma mark - WelcomeToChromeViewDelegate

- (void)welcomeToChromeViewDidTapTOSLink {
  self.didTapTOSLink = YES;
  NSString* title = l10n_util::GetNSString(IDS_IOS_FIRSTRUN_TERMS_TITLE);
  NSURL* tosUrl = [self newTermsOfServiceUrl];
  [self openStaticFileWithURL:tosUrl title:title];
}

- (void)welcomeToChromeViewDidTapPrivacyLink {
  self.didTapPrivacyLink = YES;
  NSString* title = l10n_util::GetNSString(IDS_IOS_FIRSTRUN_PRIVACY_TITLE);
  NSURL* privacyUrl = net::NSURLWithGURL(
      GURL("https://www.google.com/chrome/privacy-plain.html"));
  [self openStaticFileWithURL:privacyUrl title:title];
  [self.navigationController setDelegate:self];
}

- (void)welcomeToChromeViewDidTapOKButton:(WelcomeToChromeView*)view {
  GetApplicationContext()->GetLocalState()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, view.checkBoxSelected);

  if (view.checkBoxSelected) {
    if (self.didTapPrivacyLink) {
      UMA_HISTOGRAM_ENUMERATION("MobileFre.PrivacyLinkTappedStatus",
                                self.privacyLinkStatus,
                                NUM_MOBILE_FRE_LINK_TAPPED_STATUS);
    }
    if (self.didTapTOSLink)
      base::RecordAction(base::UserMetricsAction("MobileFreTOSLinkTapped"));
  }

  FirstRunConfiguration* firstRunConfig = [[FirstRunConfiguration alloc] init];
  bool hasSSOAccounts = ios::GetChromeBrowserProvider()
                            ->GetChromeIdentityService()
                            ->HasIdentities();
  [firstRunConfig setHasSSOAccount:hasSSOAccounts];
  FirstRunChromeSigninViewController* signInController =
      [[FirstRunChromeSigninViewController alloc]
          initWithBrowser:_browser
           firstRunConfig:firstRunConfig
           signInIdentity:nil
                presenter:self.presenter
               dispatcher:self.dispatcher];

  CATransition* transition = [CATransition animation];
  transition.duration = kFadeOutAnimationDuration;
  transition.type = kCATransitionFade;
  [self.navigationController.view.layer addAnimation:transition
                                              forKey:kCATransition];
  [self.navigationController pushViewController:signInController animated:NO];
}

#pragma mark - UINavigationControllerDelegate

- (id<UIViewControllerAnimatedTransitioning>)
           navigationController:(UINavigationController*)navigationController
animationControllerForOperation:(UINavigationControllerOperation)operation
             fromViewController:(UIViewController*)fromVC
               toViewController:(UIViewController*)toVC {
  if ([fromVC isKindOfClass:[StaticFileViewController class]]) {
    StaticFileViewController* staticViewController =
        static_cast<StaticFileViewController*>(fromVC);
    self.privacyLinkStatus = staticViewController.loadStatus;
  } else {
    NOTREACHED();
  }
  [self.navigationController setDelegate:nil];
  return nil;
}
@end
