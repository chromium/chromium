// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/lens/lens_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/lens/lens_metrics.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intents/intents_donation_helper.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/new_tab_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"
#import "ios/chrome/browser/ui/lens/features.h"
#import "ios/chrome/browser/ui/lens/lens_availability.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/lens/lens_modal_animator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/chrome/browser/web_state_list/model/web_state_dependency_installer_bridge.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/public/provider/chrome/browser/lens/lens_configuration.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

using lens::CameraOpenEntryPoint;

namespace {

// Lens results web page loading progress threshold to transition from LVF to
// results page.
static const double kLensWebPageTransitionLoadingProgressThreshold = 0.5;

}  // namespace

@interface LensCoordinator () <ChromeLensControllerDelegate,
                               LensCommands,
                               CRWWebStateObserver,
                               WebStateListObserving>

// A controller that can provide an entrypoint into Lens features.
@property(nonatomic, strong) id<ChromeLensController> lensController;

// The Lens viewController.
@property(nonatomic, strong) UIViewController* viewController;

// The animator for dismissing the Lens view.
@property(nonatomic, strong) LensModalAnimator* transitionAnimator;

// Whether or not a Lens Web page load was triggered from the Lens UI.
@property(nonatomic, assign) BOOL lensWebPageLoadTriggeredFromInputSelection;

// The WebState that is loading a Lens results page, if any.
@property(nonatomic, assign) web::WebState* loadingWebState;

// TemplateURL used to get the search engine.
@property(nonatomic, assign) TemplateURLService* templateURLService;

// Feature Engagement Tracker used to handle promo events.
@property(nonatomic, assign) feature_engagement::Tracker* tracker;

@end

@implementation LensCoordinator {
  // Used to observe the active WebState.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<base::ScopedObservation<web::WebState, web::WebStateObserver>>
      _webStateObservation;

  // Used to observe the WebStateList.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<base::ScopedObservation<WebStateList, WebStateListObserver>>
      _webStateListObservation;

  // Indicates whether the view was dismissed without a Lens search.
  BOOL _dismissed;

  // If set to YES, an IPH bubble will be presented on the NTP that points to
  // the Lens icon in the NTP fakebox, if Lens is dismissed by the user.
  BOOL _presentNTPLensIconBubbleOnDismiss;
}
@synthesize baseViewController = _baseViewController;

// The timeout before the Lens UI is closed, if the Lens Web page
// fails to load.
const base::TimeDelta kCloseLensViewTimeout = base::Seconds(10);

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

- (void)start {
  [super start];

  Browser* browser = self.browser;
  DCHECK(browser);

  [browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(LensCommands)];

  _webStateListObservation = std::make_unique<
      base::ScopedObservation<WebStateList, WebStateListObserver>>(
      _webStateListObserverBridge.get());

  _webStateObservation = std::make_unique<
      base::ScopedObservation<web::WebState, web::WebStateObserver>>(
      _webStateObserverBridge.get());

  ProfileIOS* profile = browser->GetProfile();
  DCHECK(profile);

  self.templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  self.tracker = feature_engagement::TrackerFactory::GetForProfile(profile);
  self.loadingWebState = nil;
  self.lensWebPageLoadTriggeredFromInputSelection = NO;
  self.transitionAnimator = [[LensModalAnimator alloc] init];
  _webStateListObservation->Observe(browser->GetWebStateList());
  [self updateLensAvailabilityForWidgets];
  [self updateQRCodeOrLensAppShortcutItem];
  _dismissed = NO;
  _presentNTPLensIconBubbleOnDismiss = NO;
}

- (void)stop {
  Browser* browser = self.browser;
  DCHECK(browser);

  [self dismissViewController];
  self.loadingWebState = nullptr;
  self.transitionAnimator = nil;
  self.lensWebPageLoadTriggeredFromInputSelection = NO;
  self.templateURLService = nil;
  self.tracker = nil;

  _webStateListObservation.reset();
  _webStateObservation.reset();

  [browser->GetCommandDispatcher() stopDispatchingToTarget:self];

  [super stop];
}

#pragma mark - Commands

- (void)searchImageWithLens:(SearchImageWithLensCommand*)command {
  const bool isIncognito = self.browser->GetProfile()->IsOffTheRecord();
  __weak LensCoordinator* weakSelf = self;

  LensQuery* lensQuery = [LensQuery alloc];
  lensQuery.image = command.image;
  lensQuery.isIncognito = isIncognito;
  lensQuery.entrypoint = command.entryPoint;
  lensQuery.webviewSize = [self webContentFrame].size;
  ios::provider::GenerateLensLoadParamsAsync(
      lensQuery,
      base::BindOnce(^(const web::NavigationManager::WebLoadParams params) {
        [weakSelf openWebLoadParams:params];
      }));

  if (IsSegmentationTipsManagerEnabled()) {
    [self recordLensUsage];
  }
}

- (void)openLensInputSelection:(OpenLensInputSelectionCommand*)command {
  // Cancel any omnibox editing.
  Browser* browser = self.browser;
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<OmniboxCommands> omniboxCommandsHandler =
      HandlerForProtocol(dispatcher, OmniboxCommands);
  [omniboxCommandsHandler cancelOmniboxEdit];

  // Early return if Lens is not available.
  if (!ios::provider::IsLensSupported()) {
    return;
  }

  [IntentDonationHelper donateIntent:IntentType::kStartLens];

  // Create a Lens configuration for this request.
  const LensEntrypoint entrypoint = command.entryPoint;
  ProfileIOS* profile = browser->GetProfile();
  const bool isIncognito = profile->IsOffTheRecord();
  LensConfiguration* configuration = [[LensConfiguration alloc] init];
  configuration.isIncognito = isIncognito;
  configuration.singleSignOnService =
      GetApplicationContext()->GetSingleSignOnService();
  configuration.entrypoint = entrypoint;
  configuration.localState = GetApplicationContext()->GetLocalState();

  _dismissed = NO;
  _presentNTPLensIconBubbleOnDismiss =
      command.presentNTPLensIconBubbleOnDismiss;

  // Mark IPHs as completed.
  if (entrypoint == LensEntrypoint::Keyboard) {
    feature_engagement::Tracker* featureTracker = self.tracker;
    DCHECK(featureTracker);
    featureTracker->NotifyEvent(
        feature_engagement::events::kLensButtonKeyboardUsed);
    featureTracker->Dismissed(feature_engagement::kIPHiOSLensKeyboardFeature);
  } else if (entrypoint == LensEntrypoint::NewTabPage) {
    profile->GetPrefs()->SetInteger(prefs::kNTPLensEntryPointNewBadgeShownCount,
                                    INT_MAX);
  }

  if (!isIncognito) {
    AuthenticationService* authenticationService =
        AuthenticationServiceFactory::GetForProfile(profile);
    id<SystemIdentity> identity = authenticationService->GetPrimaryIdentity(
        ::signin::ConsentLevel::kSignin);
    configuration.identity = identity;
  }

  // Set the controller.
  id<ChromeLensController> lensController =
      ios::provider::NewChromeLensController(configuration);
  DCHECK(lensController);

  self.lensController = lensController;
  lensController.delegate = self;

  // Create an input selection UIViewController and present it modally.
  CGRect contentArea = [UIScreen mainScreen].bounds;

  id<LensPresentationDelegate> delegate = self.delegate;
  if (delegate) {
    contentArea = [delegate webContentAreaForLensCoordinator:self];
  }

  UIViewController* viewController =
      [lensController inputSelectionViewController];

  // TODO(crbug.com/40235185): the returned UIViewController
  // must not be nil, remove this check once the internal
  // implementation of the method is complete.
  if (!viewController) {
    return;
  }

  self.viewController = viewController;

  // Set the transitioning delegate of the view controller to customize
  // modal dismiss animations.
  const LensModalAnimator* transitionAnimator = self.transitionAnimator;
  DCHECK(transitionAnimator);
  transitionAnimator.presentationStyle = command.presentationStyle;
  transitionAnimator.presentationCompletion = command.presentationCompletion;
  [viewController setTransitioningDelegate:transitionAnimator];

  [viewController
      setModalPresentationStyle:UIModalPresentationOverCurrentContext];

  [self.baseViewController presentViewController:viewController
                                        animated:YES
                                      completion:nil];

  switch (entrypoint) {
    case LensEntrypoint::HomeScreenWidget:
      RecordCameraOpen(CameraOpenEntryPoint::WIDGET);
      break;
    case LensEntrypoint::NewTabPage:
      RecordCameraOpen(CameraOpenEntryPoint::NEW_TAB_PAGE);
      break;
    case LensEntrypoint::Keyboard:
      RecordCameraOpen(CameraOpenEntryPoint::KEYBOARD);
      break;
    case LensEntrypoint::Spotlight:
      RecordCameraOpen(CameraOpenEntryPoint::SPOTLIGHT);
      break;
    default:
      // Do not record the camera open histogram for other entry points.
      break;
  }
  GetApplicationContext()->GetLocalState()->SetTime(prefs::kLensLastOpened,
                                                    base::Time::Now());

  if (IsSegmentationTipsManagerEnabled()) {
    [self recordLensUsage];
  }
}

#pragma mark - ChromeLensControllerDelegate

- (void)lensControllerDidTapDismissButton {
  _dismissed = YES;
  self.lensWebPageLoadTriggeredFromInputSelection = NO;
  web::WebState* loadingWebState = self.loadingWebState;
  // If there is a webstate loading Lens results underneath the Lens UI,
  // close it so we return the user to the initial state.
  if (loadingWebState) {
    const int index =
        self.browser->GetWebStateList()->GetIndexOfWebState(loadingWebState);
    self.loadingWebState = nil;
    if (index != WebStateList::kInvalidIndex) {
      self.browser->GetWebStateList()->CloseWebStateAt(
          index, WebStateList::CLOSE_USER_ACTION);
    }
  }

  [self dismissViewController];
}

- (void)lensControllerDidGenerateLoadParams:
    (const web::NavigationManager::WebLoadParams&)params {
  const __weak UIViewController* lensViewController = self.viewController;
  if (!lensViewController) {
    // If the coordinator view controller is nil, simply open the params and
    // return early.
    [self openWebLoadParams:params];
    return;
  }

  // Prepare the coordinator for dismissing the presented view controller.
  // Since we are opening Lens Web, mark the page load as triggered.
  self.lensWebPageLoadTriggeredFromInputSelection = YES;
  [self openWebLoadParams:params];

  // This function will be called when the user selects an image in Lens.
  // we should continue to display the Lens UI until the search results
  // for that image have loaded, or a timeout occurs.
  // Fallback to close the preview if the page never loads beneath.
  __weak LensCoordinator* weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        // Only dismiss the Lens view if the displayed view controller is the
        // same as the one that was displayed when the load params were
        // initially generated.
        if (weakSelf.viewController == lensViewController) {
          weakSelf.lensWebPageLoadTriggeredFromInputSelection = NO;
          [weakSelf dismissViewController];
        }
      }),
      kCloseLensViewTimeout);
}

- (void)lensControllerDidSelectURL:(NSURL*)url {
  // This method is called when the user selects a URL within the Lens UI
  // and should be treated as a link press.
  web::NavigationManager::WebLoadParams params =
      web_navigation_util::CreateWebLoadParams(
          net::GURLWithNSURL(url), ui::PAGE_TRANSITION_LINK, nullptr);
  [self openWebLoadParams:params];
  [self dismissViewController];
}

- (CGRect)webContentFrame {
  id<LensPresentationDelegate> delegate = self.delegate;
  if (delegate) {
    return [delegate webContentAreaForLensCoordinator:self];
  }

  return [UIScreen mainScreen].bounds;
}

#pragma mark - WebStateListObserving methods

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change() &&
      self.lensWebPageLoadTriggeredFromInputSelection) {
    self.loadingWebState = status.new_active_web_state;
  }
}

#pragma mark - CRWWebStateObserver methods

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  if (progress >= kLensWebPageTransitionLoadingProgressThreshold) {
    [self transitionToLensWebPageWithWebState:webState];
  }
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  [self transitionToLensWebPageWithWebState:webState];
}

// Triggers the dismissal of the Lens UI (LVF) and display of the Lens web page
// load.
- (void)transitionToLensWebPageWithWebState:(web::WebState*)webState {
  DCHECK_EQ(webState, self.loadingWebState);

  // Check if the Lens UI has not already been dismissed, loaded page is a Lens
  // Web page and we are expecting a Lens Web page load, dismiss the Lens UI.
  if (self.viewController && self.lensWebPageLoadTriggeredFromInputSelection &&
      ios::provider::IsLensWebResultsURL(webState->GetLastCommittedURL())) {
    self.lensWebPageLoadTriggeredFromInputSelection = NO;
    self.loadingWebState = nil;
    [self dismissViewController];

    // As this was a successful Lens Web results page load, trigger the toolbar
    // slide-in animation.
    CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
    id<ToolbarCommands> toolbarCommandsHandler =
        HandlerForProtocol(dispatcher, ToolbarCommands);
    [toolbarCommandsHandler triggerToolbarSlideInAnimation];
  }
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  const id<ChromeLensController> lensController = self.lensController;
  if (self.lensWebPageLoadTriggeredFromInputSelection && lensController) {
    [lensController triggerSecondaryTransitionAnimation];
  }
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(webState, self.loadingWebState);
  self.loadingWebState = nil;
}

#pragma mark - Private

// Records the usage of Google Lens with the Tips Manager. This allows the Tips
// Manager to provide relevant tips or guidance to the user about the Lens
// feature.
- (void)recordLensUsage {
  CHECK(IsSegmentationTipsManagerEnabled());

  if (!self.browser) {
    return;
  }

  TipsManagerIOS* tipsManager =
      TipsManagerIOSFactory::GetForProfile(self.browser->GetProfile());

  tipsManager->NotifySignal(
      segmentation_platform::tips_manager::signals::kLensUsed);
}

- (void)openWebLoadParams:(const web::NavigationManager::WebLoadParams&)params {
  if (!self.browser)
    return;
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  UrlLoadParams loadParams;

  // Open in the current tab if the current tab is a NTP.
  if (webState && IsUrlNtp(webState->GetLastCommittedURL())) {
    loadParams = UrlLoadParams::InCurrentTab(params);
    self.loadingWebState = webState;
  } else {
    loadParams = UrlLoadParams::InNewTab(params);
    loadParams.append_to = OpenPosition::kCurrentTab;
    loadParams.SetInBackground(NO);
  }
  loadParams.in_incognito = self.browser->GetProfile()->IsOffTheRecord();
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(loadParams);
}

- (void)dismissViewController {
  if (self.baseViewController.presentedViewController == self.viewController) {
    ProceduralBlock completion = nil;
    if (_presentNTPLensIconBubbleOnDismiss && _dismissed) {
      completion = ^{
        [self presentNTPLensIconBubble];
      };
    }
    [self.baseViewController dismissViewControllerAnimated:YES
                                                completion:completion];
  }

  self.viewController = nil;
}

- (void)setLoadingWebState:(web::WebState*)webState {
  DCHECK(_webStateObservation);
  _webStateObservation->Reset();
  _loadingWebState = webState;
  if (_loadingWebState) {
    _webStateObservation->Observe(_loadingWebState);
  }
}

- (BOOL)isGoogleDefaultSearchEngine {
  DCHECK(self.templateURLService);
  const TemplateURL* defaultURL =
      self.templateURLService->GetDefaultSearchProvider();
  BOOL isGoogleDefaultSearchProvider =
      defaultURL &&
      defaultURL->GetEngineType(self.templateURLService->search_terms_data()) ==
          SEARCH_ENGINE_GOOGLE;
  return isGoogleDefaultSearchProvider;
}

// Sets the visibility of the Lens replacement for the QR code scanner in the
// home screen widget.
- (void)updateLensAvailabilityForWidgets {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSString* enableLensInWidgetKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupEnableLensInWidget);

  // Determine the availability of the Lens entrypoint in the home screen
  // widget. We don't use LensAvailability here because the seach engine status
  // is determined elsewhere in the Extension Search Engine Data Updater.
  const bool enableLensInWidget =
      ios::provider::IsLensSupported() &&
      GetApplicationContext()->GetLocalState()->GetBoolean(
          prefs::kLensCameraAssistedSearchPolicyAllowed) &&
      !base::FeatureList::IsEnabled(kDisableLensCamera) &&
      ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET;
  [sharedDefaults setBool:enableLensInWidget forKey:enableLensInWidgetKey];

  // If the Lens entrypoint is shown, determine whether to show the color or
  // monochrome icons.
  NSString* enableColorLensAndVoiceIconsInHomeScreenWidgetKey =
      base::SysUTF8ToNSString(
          app_group::kChromeAppGroupEnableColorLensAndVoiceIconsInWidget);
  const bool enableColorLensAndVoiceIconsInHomeScreenWidget =
      base::FeatureList::IsEnabled(
          kEnableColorLensAndVoiceIconsInHomeScreenWidget);
  [sharedDefaults setBool:enableColorLensAndVoiceIconsInHomeScreenWidget
                   forKey:enableColorLensAndVoiceIconsInHomeScreenWidgetKey];
}

// Sets the app shortcut item for either the QR code scanner or Lens.
- (void)updateQRCodeOrLensAppShortcutItem {
  const bool useLens =
      lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
          LensEntrypoint::AppIconLongPress, [self isGoogleDefaultSearchEngine]);

  NSString* shortcutType;
  NSString* shortcutTitle;
  UIApplicationShortcutIcon* shortcutIcon;
  if (useLens) {
    shortcutType = @"OpenLensFromAppIconLongPress";
    shortcutTitle = l10n_util::GetNSStringWithFixup(
        IDS_IOS_APPLICATION_SHORTCUT_LENS_TITLE);
    shortcutIcon =
        [UIApplicationShortcutIcon iconWithTemplateImageName:kCameraLensSymbol];
  } else {
    shortcutType = @"OpenQRScanner";
    shortcutTitle = l10n_util::GetNSStringWithFixup(
        IDS_IOS_APPLICATION_SHORTCUT_QR_SCANNER_TITLE);
    shortcutIcon =
        [UIApplicationShortcutIcon iconWithSystemImageName:@"qrcode"];
  }
  UIApplicationShortcutItem* item =
      [[UIApplicationShortcutItem alloc] initWithType:shortcutType
                                       localizedTitle:shortcutTitle
                                    localizedSubtitle:nil
                                                 icon:shortcutIcon
                                             userInfo:nil];
  [[UIApplication sharedApplication] setShortcutItems:@[ item ]];
}

// Presents an IPH bubble that points to the Lens Icon in the NTP's Fakebox.
- (void)presentNTPLensIconBubble {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [HandlerForProtocol(dispatcher, NewTabPageCommands) presentLensIconBubble];
}
@end
