// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/prototypes/diamond/new_tab_prototype_view_controller.h"

#import "base/memory/raw_ptr.h"
#import "components/omnibox/browser/location_bar_model_impl.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/location_bar/model/web_location_bar_delegate.h"
#import "ios/chrome/browser/location_bar/model/web_location_bar_impl.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_model_delegate_ios.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_url_loader.h"
#import "ios/chrome/browser/omnibox/coordinator/omnibox_coordinator.h"
#import "ios/chrome/browser/omnibox/model/chrome_omnibox_client_ios.h"
#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_util.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/web/public/web_state.h"

namespace {

const CGFloat kOmniboxContainerHorizontalMargin = 32;
const CGFloat kOmniboxContainerVerticalMargin = 24;
const CGFloat kOmniboxContainerHeight = 54;
const CGFloat kOmniboxPopupTopMargin = 8;

const size_t kMaxURLDisplayChars = 32 * 1024;

}  // namespace

@interface NewTabPrototypeViewController () <
    OmniboxPopupPresenterDelegate,
    LocationBarModelDelegateWebStateProvider,
    LocationBarURLLoader,
    WebLocationBarDelegate>
@end

@implementation NewTabPrototypeViewController {
  OmniboxCoordinator* _omniboxCoordinator;
  UIViewController* _baseViewController;
  BOOL _isNewTabPage;
  BOOL _shouldExitTabGrid;

  raw_ptr<Browser> _browser;
  raw_ptr<ChromeOmniboxClientIOS> _omniboxClient;
  // API endpoint for omnibox.
  std::unique_ptr<WebLocationBarImpl> _locationBar;
  // Facade objects used by `_toolbarCoordinator`.
  // Must outlive `_toolbarCoordinator`.
  std::unique_ptr<LocationBarModelDelegateIOS> _locationBarModelDelegate;
  std::unique_ptr<LocationBarModel> _locationBarModel;

  UIView* _omniboxContainer;
  UIView* _omniboxPopupContainer;

  std::unique_ptr<web::WebState> _newTabWebState;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              isNewTabPage:(BOOL)isNewTabPage
                         shouldExitTabGrid:(BOOL)shouldExitTabGrid {
  self = [super init];
  if (self) {
    CHECK(IsDiamondPrototypeEnabled());
    _browser = browser;
    _baseViewController = viewController;
    _isNewTabPage = isNewTabPage;
    _shouldExitTabGrid = shouldExitTabGrid;

    ProfileIOS* profile = _browser->GetProfile();

    if (profile->IsOffTheRecord()) {
      self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
    }
    if (_isNewTabPage) {
      web::WebState::CreateParams params(_browser->GetProfile());
      _newTabWebState = web::WebState::Create(params);
      AttachTabHelpers(_newTabWebState.get());
      GURL newTabURL = GURL(kChromeUINewTabURL);
      web::NavigationManager::WebLoadParams loadParams(newTabURL);
      loadParams.virtual_url = newTabURL;
      _newTabWebState->SetKeepRenderProcessAlive(true);
      _newTabWebState->GetNavigationManager()->LoadURLWithParams(loadParams);
      // LoadIfNecessary is needed because the view is not created (but needed)
      // when loading the page.
      _newTabWebState->GetNavigationManager()->LoadIfNecessary();
    }

    _locationBar = std::make_unique<WebLocationBarImpl>(self);
    _locationBar->SetURLLoader(self);
    _locationBarModelDelegate.reset(
        new LocationBarModelDelegateIOS(self, profile));
    _locationBarModel = std::make_unique<LocationBarModelImpl>(
        _locationBarModelDelegate.get(), kMaxURLDisplayChars);

    _omniboxCoordinator = [[OmniboxCoordinator alloc]
        initWithBaseViewController:nil
                           browser:_browser
                     omniboxClient:std::make_unique<ChromeOmniboxClientIOS>(
                                       _locationBar.get(), profile,
                                       feature_engagement::TrackerFactory::
                                           GetForProfile(profile))
                     isLensOverlay:NO];
    _omniboxCoordinator.presenterDelegate = self;

    [_omniboxCoordinator start];

    if (!isNewTabPage) {
      [_omniboxCoordinator updateOmniboxState];
    }
  }
  return self;
}

- (void)dealloc {
  [_omniboxCoordinator stop];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = UIColor.clearColor;
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemChromeMaterial];
  UIVisualEffectView* blurBackground =
      [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  blurBackground.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:blurBackground];
  AddSameConstraints(self.view, blurBackground);

  _omniboxContainer = [[UIView alloc] init];
  _omniboxContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _omniboxContainer.backgroundColor = [UIColor colorNamed:kSolidWhiteColor];
  _omniboxContainer.layer.shadowColor =
      [UIColor colorNamed:kSolidBlackColor].CGColor;
  _omniboxContainer.layer.shadowOffset = CGSizeMake(0, 5);
  _omniboxContainer.layer.shadowOpacity = 0.2;
  _omniboxContainer.layer.shadowRadius = 5;
  _omniboxContainer.layer.cornerRadius = kOmniboxContainerHeight / 2.0;
  [self.view addSubview:_omniboxContainer];

  _omniboxPopupContainer = [[UIView alloc] init];
  _omniboxPopupContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_omniboxPopupContainer];

  [NSLayoutConstraint activateConstraints:@[
    [_omniboxContainer.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kOmniboxContainerHorizontalMargin],
    [self.view.trailingAnchor
        constraintEqualToAnchor:_omniboxContainer.trailingAnchor
                       constant:kOmniboxContainerHorizontalMargin],
    [_omniboxContainer.topAnchor
        constraintEqualToAnchor:self.view.topAnchor
                       constant:kOmniboxContainerVerticalMargin],
    [_omniboxContainer.heightAnchor
        constraintEqualToConstant:kOmniboxContainerHeight],

    [_omniboxPopupContainer.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_omniboxPopupContainer.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_omniboxPopupContainer.topAnchor
        constraintEqualToAnchor:_omniboxContainer.bottomAnchor
                       constant:kOmniboxPopupTopMargin],
    [_omniboxPopupContainer.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
  ]];

  [_omniboxCoordinator.managedViewController
      willMoveToParentViewController:self];
  [self addChildViewController:_omniboxCoordinator.managedViewController];

  UIView* editView = _omniboxCoordinator.editView;
  editView.translatesAutoresizingMaskIntoConstraints = NO;
  [_omniboxContainer addSubview:editView];

  [_omniboxCoordinator.managedViewController
      didMoveToParentViewController:self];

  AddSameConstraints(editView, _omniboxContainer);

  [_omniboxCoordinator focusOmnibox];
}

#pragma mark - LocationBarURLLoader

- (void)loadGURLFromLocationBar:(const GURL&)url
                               postContent:
                                   (TemplateURLRef::PostContent*)postContent
                                transition:(ui::PageTransition)transition
                               disposition:(WindowOpenDisposition)disposition
    destination_url_entered_without_scheme:
        (bool)destination_url_entered_without_scheme {
  if (url.SchemeIs(url::kJavaScriptScheme)) {
    // Not managed in the proto.
    return;
  } else {
    web::NavigationManager::WebLoadParams web_params =
        web_navigation_util::CreateWebLoadParams(url, transition, postContent);
    if (destination_url_entered_without_scheme) {
      web_params.https_upgrade_type = web::HttpsUpgradeType::kOmnibox;
    }
    NSMutableDictionary<NSString*, NSString*>* combinedExtraHeaders =
        [web_navigation_util::VariationHeadersForURL(
            url, _browser->GetProfile()->IsOffTheRecord()) mutableCopy];
    [combinedExtraHeaders addEntriesFromDictionary:web_params.extra_headers];
    web_params.extra_headers = [combinedExtraHeaders copy];
    UrlLoadParams params = UrlLoadParams::InNewTab(web_params);
    params.disposition = disposition;
    params.in_incognito = _browser->GetProfile()->IsOffTheRecord();
    if (_isNewTabPage &&
        params.disposition == WindowOpenDisposition::CURRENT_TAB) {
      params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    }
    UrlLoadingBrowserAgent::FromBrowser(_browser)->Load(params);
  }

  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];

  if (_shouldExitTabGrid) {
    id<TabGridCommands> tabGridHandler =
        HandlerForProtocol(_browser->GetCommandDispatcher(), TabGridCommands);
    [tabGridHandler exitTabGrid];
  }
}

#pragma mark - LocationBarModelDelegateWebStateProvider

- (web::WebState*)webStateForLocationBarModelDelegate:
    (const LocationBarModelDelegateIOS*)locationBarModelDelegate {
  return [self webState];
}

#pragma mark - WebLocationBarDelegate

- (web::WebState*)webState {
  if (_isNewTabPage) {
    return _newTabWebState.get();
  }
  return _browser->GetWebStateList()->GetActiveWebState();
}

- (LocationBarModel*)locationBarModel {
  return _locationBarModel.get();
}

#pragma mark - OmniboxPopupPresenterDelegate

- (UIView*)popupParentViewForPresenter:(OmniboxPopupPresenter*)presenter {
  return _omniboxPopupContainer;
}

- (UIViewController*)popupParentViewControllerForPresenter:
    (OmniboxPopupPresenter*)presenter {
  return self;
}

- (UIColor*)popupBackgroundColorForPresenter:(OmniboxPopupPresenter*)presenter {
  return UIColor.clearColor;
}

- (GuideName*)omniboxGuideNameForPresenter:(OmniboxPopupPresenter*)presenter {
  return nil;
}

- (void)popupDidOpenForPresenter:(OmniboxPopupPresenter*)presenter {
}

- (void)popupDidCloseForPresenter:(OmniboxPopupPresenter*)presenter {
}

@end
