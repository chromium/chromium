// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_coordinator_app_interface.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_browser_agent.h"
#import "ios/chrome/browser/history/ui_bundled/history_coordinator.h"
#import "ios/chrome/browser/history/ui_bundled/stub_history_coordinator_delegate.h"
#import "ios/chrome/browser/main/model/browser_impl.h"
#import "ios/chrome/browser/omnibox/eg_tests/inttest/omnibox_inttest_coordinator.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/popup_menu_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/snackbar/ui_bundled/snackbar_coordinator.h"
#import "ios/chrome/browser/snackbar/ui_bundled/stub_snackbar_coordinator_delegate.h"
#import "ios/chrome/browser/tips_notifications/coordinator/enhanced_safe_browsing_promo_coordinator.h"
#import "ios/chrome/browser/tips_notifications/coordinator/lens_promo_coordinator.h"
#import "ios/chrome/browser/tips_notifications/coordinator/search_what_you_see_promo_coordinator.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/recording_command_dispatcher.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#pragma mark - ChromeCoordinatorAppInterfaceHelper

// An object that owns an instance of a ChromeCoordinator, and all the related
// dependencies (Browser, CommandDispatcher, etc).
@interface ChromeCoordinatorAppInterfaceHelper : NSObject

@property(class, readonly) ChromeCoordinatorAppInterfaceHelper* sharedInstance;

@property(nonatomic, strong) ChromeCoordinator* coordinator;
@property(nonatomic, readonly) Browser* browser;
@property(nonatomic, readonly) RecordingCommandDispatcher* dispatcher;
@property(nonatomic, readonly) UIViewController* rootViewController;

// Can be used to store a reference to a mock object or delegate that may be
// needed to test some coordinators.
@property(nonatomic, strong) id mockObject;

// Returns the shared instance, which holds references to the root view
// controller, browser, and the coordinator.
+ (instancetype)sharedInstance;

// Instantiates a "real" `BrowserImpl`, rather than a `TestBrowser`.
- (void)useNonTestBrowser;

// Instantiates a TestBrowser.
- (void)useTestBrowser;

// Dismisses the root view controller, stops the coordinator, and clears the
// browser.
- (void)reset;

@end

@implementation ChromeCoordinatorAppInterfaceHelper {
  std::unique_ptr<Browser> _browser;
  UIViewController* _rootViewController;
}

- (instancetype)init {
  self = [super init];
  return self;
}

+ (instancetype)sharedInstance {
  static ChromeCoordinatorAppInterfaceHelper* instance;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    instance = [[ChromeCoordinatorAppInterfaceHelper alloc] init];
    [instance reset];
  });
  return instance;
}

- (void)reset {
  [_rootViewController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:nil];
  _rootViewController = nil;
  _mockObject = nil;
  _browser.reset();
  _dispatcher = [[RecordingCommandDispatcher alloc] init];
}

- (Browser*)browser {
  if (!_browser) {
    // Use a `TestBrowser` by default.
    [self useTestBrowser];
  }
  return _browser.get();
}

- (UIViewController*)rootViewController {
  if (_rootViewController) {
    return _rootViewController;
  }
  _rootViewController = [[UIViewController alloc] init];
  _rootViewController.modalPresentationStyle = UIModalPresentationFullScreen;
  _rootViewController.view.backgroundColor = [UIColor whiteColor];
  [[ChromeEarlGreyAppInterface keyWindow].rootViewController
      presentViewController:_rootViewController
                   animated:NO
                 completion:nil];
  return _rootViewController;
}

- (void)useNonTestBrowser {
  _browser = std::make_unique<BrowserImpl>(
      chrome_test_util::GetOriginalProfile(),
      chrome_test_util::GetForegroundActiveScene(), _dispatcher,
      /*active_browser=*/nullptr,
      BrowserImpl::InsertionPolicy::kAttachTabHelpers,
      BrowserImpl::ActivationPolicy::kForceRealization,
      BrowserImpl::Type::kRegular);
  UrlLoadingBrowserAgent::RemoveFromBrowser(_browser.get());
  FakeUrlLoadingBrowserAgent::InjectForBrowser(_browser.get());
  [self insertInitialWebstate];
}

- (void)useTestBrowser {
  std::unique_ptr<TestBrowser> testBrowser = std::make_unique<TestBrowser>(
      chrome_test_util::GetOriginalProfile(),
      chrome_test_util::GetForegroundActiveScene());
  testBrowser->SetCommandDispatcher(_dispatcher);
  _browser = std::move(testBrowser);
  UrlLoadingNotifierBrowserAgent::CreateForBrowser(_browser.get());
  FakeUrlLoadingBrowserAgent::InjectForBrowser(_browser.get());
  [self insertInitialWebstate];
}

- (void)setCoordinator:(ChromeCoordinator*)coordinator {
  // Don't allow setting a coordinator if one is already set. Do allow setting
  // it to nil though.
  CHECK(!_coordinator || !coordinator);
  _coordinator = coordinator;
}

#pragma mark - ChromeCoordinatorAppInterfaceHelper private methods

// Inserts a webstate into the browser since many coordinators do not expect
// an empty WebStateList.
- (void)insertInitialWebstate {
  web::WebState::CreateParams params(_browser->GetProfile());
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);
  _browser->GetWebStateList()->InsertWebState(
      std::move(webState),
      WebStateList::InsertionParams::Automatic().Activate());
}

@end

#pragma mark - ChromeCoordinatorAppInterface

@interface ChromeCoordinatorAppInterface ()
@property(class, readonly) ChromeCoordinatorAppInterfaceHelper* helper;
@end

@implementation ChromeCoordinatorAppInterface

+ (BOOL)selectorWasDispatched:(NSString*)selectorString {
  return [self.helper.dispatcher.dispatches containsObject:selectorString];
}

+ (void)dispatchSelector:(NSString*)selectorString {
  SEL selector = NSSelectorFromString(selectorString);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
  [self.helper.dispatcher performSelector:selector];
#pragma clang diagnostic pop
}

+ (void)setAction:(ProceduralBlock)block forSelector:(NSString*)selectorString {
  SEL selector = NSSelectorFromString(selectorString);
  [self.helper.dispatcher setAction:block forSelector:selector];
}

+ (void)stopCoordinator {
  [self.helper.coordinator stop];
  self.helper.coordinator = nil;
}

+ (void)reset {
  [self stopCoordinator];
  [self.helper reset];
}

#pragma mark - Properties

+ (CommandDispatcher*)dispatcher {
  return self.helper.dispatcher;
}

+ (ChromeCoordinator*)coordinator {
  return self.helper.coordinator;
}

+ (NSURL*)lastURLLoaded {
  FakeUrlLoadingBrowserAgent* URLLoader =
      FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
          UrlLoadingBrowserAgent::FromBrowser(self.helper.browser));
  return net::NSURLWithGURL(URLLoader->last_params.web_params.url);
}

+ (BOOL)lastURLLoadedInIncognito {
  FakeUrlLoadingBrowserAgent* URLLoader =
      FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
          UrlLoadingBrowserAgent::FromBrowser(self.helper.browser));
  return URLLoader->last_params.in_incognito;
}

#pragma mark - Methods to start coordinators

+ (void)startLensPromoCoordinator {
  self.helper.coordinator = [[LensPromoCoordinator alloc]
      initWithBaseViewController:[self rootViewController]
                         browser:self.helper.browser];
  [self.helper.coordinator start];
}

+ (void)startEnhancedSafeBrowsingPromoCoordinator {
  self.helper.coordinator = [[EnhancedSafeBrowsingPromoCoordinator alloc]
      initWithBaseViewController:[self rootViewController]
                         browser:self.helper.browser];
  [self.helper.coordinator start];
}

+ (void)startHistoryCoordinator {
  HistoryCoordinator* coordinator = [[HistoryCoordinator alloc]
      initWithBaseViewController:[self rootViewController]
                         browser:self.helper.browser];
  self.helper.mockObject = [[StubHistoryCoordinatorDelegate alloc] init];
  coordinator.delegate = self.helper.mockObject;

  self.helper.coordinator = coordinator;
  [self.helper.coordinator start];
}

+ (void)startPopupMenuCoordinator {
  // The IOSLanguageDetectionTabHelper is required by the PopupMenu.
  PrefService* prefs = self.helper.browser->GetProfile()->GetPrefs();
  language::IOSLanguageDetectionTabHelper::CreateForWebState(
      self.helper.browser->GetWebStateList()->GetWebStateAt(0),
      /*url_language_histogram=*/nullptr, nullptr, prefs);

  PopupMenuCoordinator* coordinator =
      [[PopupMenuCoordinator alloc] initWithBrowser:self.helper.browser];
  coordinator.baseViewController = [self rootViewController];
  self.helper.coordinator = coordinator;
  [self.helper.coordinator start];
}

+ (void)startOmniboxCoordinator {
  AutocompleteBrowserAgent::CreateForBrowser(self.helper.browser);
  OmniboxInttestCoordinator* coordinator = [[OmniboxInttestCoordinator alloc]
      initWithBaseViewController:[self rootViewController]
                         browser:self.helper.browser];
  self.helper.coordinator = coordinator;
  [self.helper.coordinator start];
}

+ (void)startSearchWhatYouSeePromoCoordinator {
  self.helper.coordinator = [[SearchWhatYouSeePromoCoordinator alloc]
      initWithBaseViewController:[self rootViewController]
                         browser:self.helper.browser];
  [self.helper.coordinator start];
}

+ (void)startSnackbarCoordinator {
  self.helper.mockObject = [[StubSnackbarCoordinatorDelegate alloc] init];
  self.helper.coordinator = [[SnackbarCoordinator alloc]
      initWithBaseViewController:[self rootViewController]
                         browser:self.helper.browser
                        delegate:self.helper.mockObject];
  [self.helper.coordinator start];
}

#pragma mark - Private

+ (ChromeCoordinatorAppInterfaceHelper*)helper {
  return ChromeCoordinatorAppInterfaceHelper.sharedInstance;
}

+ (UIViewController*)rootViewController {
  return self.helper.rootViewController;
}

@end
