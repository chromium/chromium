// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/coordinator/popup/omnibox_popup_coordinator.h"

#import "base/feature_list.h"
#import "components/favicon/core/large_icon_service.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/history/core/browser/top_sites.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/autocomplete/model/remote_suggestions_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_cache_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/favicon/ui_bundled/favicon_attributes_provider.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/history/model/top_sites_factory.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/omnibox/coordinator/popup/omnibox_popup_mediator.h"
#import "ios/chrome/browser/omnibox/debugger/omnibox_debugger_mediator.h"
#import "ios/chrome/browser/omnibox/debugger/omnibox_debugger_view_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_image_fetcher.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/omnibox/ui/popup/carousel/carousel_item.h"
#import "ios/chrome/browser/omnibox/ui/popup/carousel/carousel_item_menu_provider.h"
#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_view_controller.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_coordinator.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_params.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/device_form_factor.h"

@interface OmniboxPopupCoordinator () <OmniboxPopupMediatorProtocolProvider,
                                       OmniboxPopupMediatorSharingDelegate>

@property(nonatomic, strong) OmniboxPopupViewController* popupViewController;
@property(nonatomic, strong) OmniboxPopupMediator* mediator;
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;

// Owned by OmniboxAutocompleteController.
@property(nonatomic, assign) AutocompleteController* autocompleteController;

@end

@implementation OmniboxPopupCoordinator {
  /// The omnibox autocomplete controller.
  __weak OmniboxAutocompleteController* _omniboxAutocompleteController;
  /// The omnibox debugger mediator.
  OmniboxDebuggerMediator* _omniboxDebuggerMediator;
  /// The omnibox image fetcher.
  OmniboxImageFetcher* _omniboxImageFetcher;
  /// The context in which the omnibox is presented.
  OmniboxPresentationContext _presentationContext;
}

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                    autocompleteController:
                        (AutocompleteController*)autocompleteController
             omniboxAutocompleteController:
                 (OmniboxAutocompleteController*)omniboxAutocompleteController
                       presentationContext:
                           (OmniboxPresentationContext)presentationContext {
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    DCHECK(autocompleteController);
    _autocompleteController = autocompleteController;
    _popupViewController = [[OmniboxPopupViewController alloc]
        initWithPresentationContext:presentationContext];
    _KeyboardDelegate = _popupViewController;
    _omniboxAutocompleteController = omniboxAutocompleteController;
    _presentationContext = presentationContext;
  }
  return self;
}

- (void)start {
  std::unique_ptr<image_fetcher::ImageDataFetcher> imageFetcher =
      std::make_unique<image_fetcher::ImageDataFetcher>(
          self.profile->GetSharedURLLoaderFactory());

  _omniboxImageFetcher = [[OmniboxImageFetcher alloc]
      initWithFaviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(
                                self.profile)
               imageFetcher:std::move(imageFetcher)];

  BOOL isIncognito = self.profile->IsOffTheRecord();

  self.mediator = [[OmniboxPopupMediator alloc]
          initWithTracker:feature_engagement::TrackerFactory::GetForProfile(
                              self.profile)
      omniboxImageFetcher:_omniboxImageFetcher];

  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(self.profile);
  self.mediator.defaultSearchEngineIsGoogle =
      templateURLService && templateURLService->GetDefaultSearchProvider() &&
      templateURLService->GetDefaultSearchProvider()->GetEngineType(
          templateURLService->search_terms_data()) == SEARCH_ENGINE_GOOGLE;
  self.mediator.templateURLService = templateURLService;
  self.mediator.protocolProvider = self;
  self.mediator.sharingDelegate = self;
  BrowserActionFactory* actionFactory = [[BrowserActionFactory alloc]
      initWithBrowser:self.browser
             scenario:kMenuScenarioHistogramOmniboxMostVisitedEntry];
  self.mediator.mostVisitedActionFactory = actionFactory;
  self.mediator.omniboxAutocompleteController = _omniboxAutocompleteController;
  self.popupViewController.imageRetriever = self.mediator;
  self.popupViewController.faviconRetriever = self.mediator;
  self.popupViewController.mutator = self.mediator;
  self.popupViewController.incognito = isIncognito;
  favicon::LargeIconService* largeIconService =
      IOSChromeLargeIconServiceFactory::GetForProfile(self.profile);
  LargeIconCache* cache =
      IOSChromeLargeIconCacheFactory::GetForProfile(self.profile);
  self.popupViewController.largeIconService = largeIconService;
  self.popupViewController.largeIconCache = cache;
  self.popupViewController.carouselMenuProvider = self.mediator;
  self.popupViewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);

  self.mediator.consumer = self.popupViewController;
  self.mediator.carouselItemConsumer = self.popupViewController;
  self.mediator.allowIncognitoActions =
      !IsIncognitoModeDisabled(self.profile->GetPrefs());

  _omniboxAutocompleteController.delegate = self.mediator;

  self.mediator.applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  self.mediator.omniboxCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  self.mediator.incognito = isIncognito;
  self.mediator.sceneState = self.browser->GetSceneState();
  self.mediator.presenter = [[OmniboxPopupPresenter alloc]
      initWithPopupPresenterDelegate:self.presenterDelegate
                 popupViewController:self.popupViewController
                   layoutGuideCenter:LayoutGuideCenterForBrowser(self.browser)
                           incognito:isIncognito
                 presentationContext:_presentationContext];

  if (experimental_flags::IsOmniboxDebuggingEnabled()) {
    [self setupDebug];
  }
}

- (void)stop {
  [_omniboxDebuggerMediator disconnect];
  _omniboxDebuggerMediator = nil;

  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;

  self.popupViewController = nil;
  self.mediator = nil;
  self.autocompleteController = nullptr;
  _omniboxImageFetcher = nil;
}

- (BOOL)isOpen {
  return _omniboxAutocompleteController.hasSuggestions;
}

- (id<ToolbarOmniboxConsumer>)toolbarOmniboxConsumer {
  return self.mediator.presenter;
}

- (void)toggleOmniboxDebuggerView {
  CHECK(experimental_flags::IsOmniboxDebuggingEnabled());
  [self.popupViewController toggleOmniboxDebuggerView];
}

#pragma mark - Property accessor

- (BOOL)hasResults {
  return _omniboxAutocompleteController.hasSuggestions;
}

#pragma mark - OmniboxPopupMediatorProtocolProvider

- (scoped_refptr<history::TopSites>)topSites {
  return ios::TopSitesFactory::GetForProfile(self.profile);
}

- (id<SnackbarCommands>)snackbarCommandsHandler {
  return HandlerForProtocol(self.browser->GetCommandDispatcher(),
                            SnackbarCommands);
}

#pragma mark - OmniboxPopupMediatorSharingDelegate

/// Triggers the URL sharing flow for the given `URL` and `title`, with the
/// origin `view` representing the UI component for that URL.
- (void)popupMediator:(OmniboxPopupMediator*)mediator
             shareURL:(GURL)URL
                title:(NSString*)title
           originView:(UIView*)originView {
  SharingParams* params = [[SharingParams alloc]
      initWithURL:URL
            title:title
         scenario:SharingScenario::OmniboxMostVisitedEntry];
  self.sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.popupViewController
                         browser:self.browser
                          params:params
                      originView:originView];
  [self.sharingCoordinator start];
}

#pragma mark - private

- (void)setupDebug {
  DCHECK(experimental_flags::IsOmniboxDebuggingEnabled());

  RemoteSuggestionsService* remoteSuggestionsService =
      RemoteSuggestionsServiceFactory::GetForProfile(
          self.profile, /*create_if_necessary=*/true);

  _omniboxDebuggerMediator = [[OmniboxDebuggerMediator alloc]
      initWithAutocompleteController:_autocompleteController
            remoteSuggestionsService:remoteSuggestionsService];

  PopupDebugInfoViewController* viewController =
      [[PopupDebugInfoViewController alloc] init];
  _omniboxDebuggerMediator.consumer = viewController;
  viewController.mutator = _omniboxDebuggerMediator;

  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  self.popupViewController.debugInfoViewController = navController;
}

@end
