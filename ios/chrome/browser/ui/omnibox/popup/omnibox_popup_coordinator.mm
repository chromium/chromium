// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_coordinator.h"

#import "base/feature_list.h"
#import "components/favicon/core/large_icon_service.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/history/core/browser/top_sites.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/autocomplete/model/remote_suggestions_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_cache_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/favicon/ui_bundled/favicon_attributes_provider.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/history/model/top_sites_factory.h"
#import "ios/chrome/browser/net/model/crurl.h"
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
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/carousel/carousel_item.h"
#import "ios/chrome/browser/ui/omnibox/popup/carousel/carousel_item_menu_provider.h"
#import "ios/chrome/browser/ui/omnibox/popup/content_providing.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_pedal_annotator.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_mediator.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_controller.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_ios.h"
#import "ios/chrome/browser/ui/omnibox/popup/pedal_section_extractor.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_debug_info_view_controller.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/device_form_factor.h"

@interface OmniboxPopupCoordinator () <OmniboxPopupMediatorProtocolProvider,
                                       OmniboxPopupMediatorSharingDelegate> {
  std::unique_ptr<OmniboxPopupViewIOS> _popupView;
}

@property(nonatomic, strong) OmniboxPopupViewController* popupViewController;
@property(nonatomic, strong) OmniboxPopupMediator* mediator;
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;

// Owned by OmniboxEditModel.
@property(nonatomic, assign) AutocompleteController* autocompleteController;

@end

@implementation OmniboxPopupCoordinator

#pragma mark - Public

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
        autocompleteController:(AutocompleteController*)autocompleteController
                     popupView:(std::unique_ptr<OmniboxPopupViewIOS>)popupView {
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    DCHECK(autocompleteController);
    _autocompleteController = autocompleteController;
    _popupView = std::move(popupView);
    _popupViewController = [[OmniboxPopupViewController alloc] init];
    _popupReturnDelegate = _popupViewController;
    _KeyboardDelegate = _popupViewController;
  }
  return self;
}

- (void)start {
  std::unique_ptr<image_fetcher::ImageDataFetcher> imageFetcher =
      std::make_unique<image_fetcher::ImageDataFetcher>(
          self.browser->GetProfile()->GetSharedURLLoaderFactory());

  BOOL isIncognito = self.browser->GetProfile()->IsOffTheRecord();

  RemoteSuggestionsService* remoteSuggestionsService =
      RemoteSuggestionsServiceFactory::GetForProfile(
          self.browser->GetProfile(), /*create_if_necessary=*/true);

  self.mediator = [[OmniboxPopupMediator alloc]
               initWithFetcher:std::move(imageFetcher)
                 faviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(
                                   self.browser->GetProfile())
        autocompleteController:self.autocompleteController
      remoteSuggestionsService:remoteSuggestionsService
                      delegate:_popupView.get()
                       tracker:feature_engagement::TrackerFactory::
                                   GetForProfile(self.browser->GetProfile())];

  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(self.browser->GetProfile());
  self.mediator.defaultSearchEngineIsGoogle =
      templateURLService && templateURLService->GetDefaultSearchProvider() &&
      templateURLService->GetDefaultSearchProvider()->GetEngineType(
          templateURLService->search_terms_data()) == SEARCH_ENGINE_GOOGLE;
  self.mediator.protocolProvider = self;
  self.mediator.sharingDelegate = self;
  BrowserActionFactory* actionFactory = [[BrowserActionFactory alloc]
      initWithBrowser:self.browser
             scenario:kMenuScenarioHistogramOmniboxMostVisitedEntry];
  self.mediator.mostVisitedActionFactory = actionFactory;
  self.popupViewController.imageRetriever = self.mediator;
  self.popupViewController.faviconRetriever = self.mediator;
  self.popupViewController.delegate = self.mediator;
  self.popupViewController.dataSource = self.mediator;
  self.popupViewController.incognito = isIncognito;
  favicon::LargeIconService* largeIconService =
      IOSChromeLargeIconServiceFactory::GetForProfile(
          self.browser->GetProfile());
  LargeIconCache* cache =
      IOSChromeLargeIconCacheFactory::GetForProfile(self.browser->GetProfile());
  self.popupViewController.largeIconService = largeIconService;
  self.popupViewController.largeIconCache = cache;
  self.popupViewController.carouselMenuProvider = self.mediator;
  self.popupViewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);

  self.mediator.consumer = self.popupViewController;
  self.popupViewController.matchPreviewDelegate =
      self.popupMatchPreviewDelegate;
  self.popupViewController.acceptReturnDelegate = self.acceptReturnDelegate;
  self.mediator.carouselItemConsumer = self.popupViewController;
  self.mediator.allowIncognitoActions =
      !IsIncognitoModeDisabled(self.browser->GetProfile()->GetPrefs());

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  OmniboxPedalAnnotator* annotator = [[OmniboxPedalAnnotator alloc] init];
  annotator.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  annotator.settingsHandler = HandlerForProtocol(dispatcher, SettingsCommands);
  annotator.omniboxHandler = HandlerForProtocol(dispatcher, OmniboxCommands);
  annotator.quickDeleteHandler =
      HandlerForProtocol(dispatcher, QuickDeleteCommands);

  self.mediator.pedalAnnotator = annotator;

  self.mediator.applicationCommandsHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  self.mediator.incognito = isIncognito;
  self.mediator.sceneState = self.browser->GetSceneState();
  self.mediator.presenter = [[OmniboxPopupPresenter alloc]
      initWithPopupPresenterDelegate:self.presenterDelegate
                 popupViewController:self.popupViewController
                   layoutGuideCenter:LayoutGuideCenterForBrowser(self.browser)
                           incognito:isIncognito];

  _popupView->SetMediator(self.mediator);

  if (experimental_flags::IsOmniboxDebuggingEnabled()) {
    [self setupDebug];
  }
}

- (void)stop {
  [self.mediator disconnect];

  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;
  _popupView.reset();
}

- (BOOL)isOpen {
  return self.mediator.isOpen;
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
  return self.mediator.hasResults;
}

#pragma mark - OmniboxPopupMediatorProtocolProvider

- (scoped_refptr<history::TopSites>)topSites {
  return ios::TopSitesFactory::GetForProfile(self.browser->GetProfile());
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

  PopupDebugInfoViewController* viewController =
      [[PopupDebugInfoViewController alloc] init];
  self.mediator.debugInfoConsumer = viewController;

  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  self.popupViewController.debugInfoViewController = navController;
}

@end
