// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/adaptive_toolbar_mediator.h"

#import "base/containers/contains.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/collaboration/public/messaging/message.h"
#import "components/collaboration/public/messaging/messaging_backend_service.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/collaboration/model/messaging/messaging_backend_service_bridge.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_availability.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer_bridge.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/load_query_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_tab_group_state.h"
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/tab_group_indicator_features_utils.h"
#import "ios/chrome/browser/toolbar/ui_bundled/toolbar_consumer.h"
#import "ios/chrome/browser/url_loading/model/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"
#import "ios/web/public/favicon/favicon_status.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/image/image.h"

namespace {

// Returns a local tab group ID in `message`. Returns nullopt if the ID doesn't
// exist.
std::optional<tab_groups::LocalTabGroupID> LocalTabGroupID(
    collaboration::messaging::PersistentMessage message) {
  if (!message.attribution.tab_group_metadata.has_value()) {
    return std::nullopt;
  }
  collaboration::messaging::TabGroupMessageMetadata group_data =
      message.attribution.tab_group_metadata.value();
  return group_data.local_tab_group_id;
}

}  // namespace

@interface AdaptiveToolbarMediator () <CRWWebStateObserver,
                                       MessagingBackendServiceObserving,
                                       OverlayPresenterObserving,
                                       WebStateListObserving>

/// The current web state associated with the toolbar.
@property(nonatomic, assign) web::WebState* webState;

/// Whether an overlay is currently presented over the web content area.
@property(nonatomic, assign, getter=isWebContentAreaShowingOverlay)
    BOOL webContentAreaShowingOverlay;

@end

@implementation AdaptiveToolbarMediator {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<OverlayPresenterObserverBridge> _overlayObserver;

  // A service to get activity messages for a shared tab group.
  raw_ptr<collaboration::messaging::MessagingBackendService> _messagingService;
  // The bridge between the C++ MessagingBackendService observer and this
  // Objective-C class.
  std::unique_ptr<MessagingBackendServiceBridge> _messagingBackendServiceBridge;
  // A set of a shared group ID that has changed and a user has not seen it yet.
  std::set<tab_groups::LocalTabGroupID> _dirtyGroups;
}

- (instancetype)initWithMessagingService:
    (collaboration::messaging::MessagingBackendService*)messagingService {
  self = [super init];
  if (self) {
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _overlayObserver = std::make_unique<OverlayPresenterObserverBridge>(self);

    _messagingService = messagingService;
    if (_messagingService) {
      _messagingBackendServiceBridge =
          std::make_unique<MessagingBackendServiceBridge>(self);
      _messagingService->AddPersistentMessageObserver(
          _messagingBackendServiceBridge.get());
      [self fetchMessages];
    }
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

#pragma mark - Public

- (void)updateConsumerForWebState:(web::WebState*)webState {
  [self updateNavigationBackAndForwardStateForWebState:webState];
  [self updateShareMenuForWebState:webState];
}

- (void)disconnect {
  self.webContentAreaOverlayPresenter = nullptr;
  self.navigationBrowserAgent = nullptr;

  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }

  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    _webState = nullptr;
  }

  if (_messagingService) {
    _messagingService->RemovePersistentMessageObserver(
        _messagingBackendServiceBridge.get());
    _messagingBackendServiceBridge.reset();
    _messagingService = nullptr;
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  DCHECK_EQ(_webState, webState);
  [self.consumer setLoadingProgressFraction:progress];
}

- (void)webStateDidChangeBackForwardState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _webState->RemoveObserver(_webStateObserver.get());
  _webState = nullptr;
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);

  if (status.active_web_state_change()) {
    self.webState = status.new_active_web_state;
  }

  if (webStateList->IsBatchInProgress()) {
    return;
  }

  [self updateConsumerTabGroupState];

  const int tabCount = [self tabCountToDisplay];
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      [self.consumer setTabCount:tabCount addedInBackground:NO];
      break;
    case WebStateListChange::Type::kDetach:
      [self.consumer setTabCount:tabCount addedInBackground:NO];
      break;
    case WebStateListChange::Type::kMove:
      [self.consumer setTabCount:tabCount addedInBackground:NO];
      break;
    case WebStateListChange::Type::kReplace:
      // Do nothing when a WebState is replaced.
      break;
    case WebStateListChange::Type::kInsert:
      [self.consumer setTabCount:tabCount
               addedInBackground:!status.active_web_state_change()];
      break;
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
  }
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);
  [self updateConsumerTabGroupState];
  [self.consumer setTabCount:[self tabCountToDisplay] addedInBackground:NO];
}

#pragma mark - MessagingBackendServiceObserving

- (void)onMessagingBackendServiceInitialized {
  [self fetchMessages];
}

- (void)displayPersistentMessage:
    (collaboration::messaging::PersistentMessage)message {
  CHECK(_messagingService);
  CHECK(_messagingService->IsInitialized());

  if (message.type !=
      collaboration::messaging::PersistentNotificationType::DIRTY_TAB_GROUP) {
    return;
  }

  if (std::optional<tab_groups::LocalTabGroupID> localTabGroupID =
          LocalTabGroupID(message)) {
    _dirtyGroups.insert(*localTabGroupID);
  }

  [self updateTabGridButtonBlueDot];
}

- (void)hidePersistentMessage:
    (collaboration::messaging::PersistentMessage)message {
  CHECK(_messagingService);
  CHECK(_messagingService->IsInitialized());

  if (message.type !=
      collaboration::messaging::PersistentNotificationType::DIRTY_TAB_GROUP) {
    return;
  }

  if (std::optional<tab_groups::LocalTabGroupID> localTabGroupID =
          LocalTabGroupID(message)) {
    _dirtyGroups.erase(*localTabGroupID);
  }

  [self updateTabGridButtonBlueDot];
}

#pragma mark - AdaptiveToolbarMenusProvider

- (UIMenu*)menuForButtonOfType:(AdaptiveToolbarButtonType)buttonType {
  switch (buttonType) {
    case AdaptiveToolbarButtonTypeBack:
      return [self menuForNavigationItems:self.webState->GetNavigationManager()
                                              ->GetBackwardItems()];

    case AdaptiveToolbarButtonTypeForward:
      return [self menuForNavigationItems:self.webState->GetNavigationManager()
                                              ->GetForwardItems()];

    case AdaptiveToolbarButtonTypeNewTab:
      return [self menuForNewTabButton];

    case AdaptiveToolbarButtonTypeTabGrid:
      return [self menuForTabGridButton];
  }
  return nil;
}

#pragma mark - Setters

- (void)setIncognito:(BOOL)incognito {
  if (incognito == _incognito) {
    return;
  }

  _incognito = incognito;
}

- (void)setWebState:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
  }

  _webState = webState;

  if (_webState) {
    _webState->AddObserver(_webStateObserver.get());

    if (self.consumer) {
      [self updateConsumer];
    }

    [self updateTabGridButtonBlueDot];
  }
}

- (void)setConsumer:(id<ToolbarConsumer>)consumer {
  _consumer = consumer;
  [_consumer setVoiceSearchEnabled:ios::provider::IsVoiceSearchEnabled()];
  if (self.webState) {
    [self updateConsumer];
  }
  if (self.webStateList) {
    [self.consumer setTabCount:_webStateList->count() addedInBackground:NO];
    [self updateTabGridButtonBlueDot];
  }
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  _webStateList = webStateList;

  if (_webStateList) {
    self.webState = _webStateList->GetActiveWebState();
    _webStateList->AddObserver(_webStateListObserver.get());

    if (self.consumer) {
      [self.consumer setTabCount:_webStateList->count() addedInBackground:NO];
      [self updateTabGridButtonBlueDot];
    }
  } else {
    // Clear the web navigation browser agent if the webStateList is nil.
    self.webState = nil;
    self.navigationBrowserAgent = nil;
  }

  [self fetchMessages];
}

- (void)setWebContentAreaOverlayPresenter:
    (OverlayPresenter*)webContentAreaOverlayPresenter {
  if (_webContentAreaOverlayPresenter) {
    _webContentAreaOverlayPresenter->RemoveObserver(_overlayObserver.get());
  }

  _webContentAreaOverlayPresenter = webContentAreaOverlayPresenter;

  if (_webContentAreaOverlayPresenter) {
    _webContentAreaOverlayPresenter->AddObserver(_overlayObserver.get());
  }
}

- (void)setWebContentAreaShowingOverlay:(BOOL)webContentAreaShowingOverlay {
  if (_webContentAreaShowingOverlay == webContentAreaShowingOverlay) {
    return;
  }
  _webContentAreaShowingOverlay = webContentAreaShowingOverlay;
  [self updateShareMenuForWebState:self.webState];
}

#pragma mark - Update helper methods

/// Updates the consumer Tab Group state.
- (void)updateConsumerTabGroupState {
  [self.consumer updateTabGroupState:[self tabGroupStateToDisplay]];
  [self updateTabGridButtonBlueDot];
}

/// Updates the consumer to match the current WebState.
- (void)updateConsumer {
  DCHECK(self.webState);
  DCHECK(self.consumer);
  [self updateConsumerForWebState:self.webState];

  BOOL isNTP = IsVisibleURLNewTabPage(self.webState);
  [self.consumer setIsNTP:isNTP];
  // Never show the loading UI for an NTP.
  BOOL isLoading = self.webState->IsLoading() && !isNTP;
  [self.consumer setLoadingState:isLoading];
  if (isLoading) {
    [self.consumer
        setLoadingProgressFraction:self.webState->GetLoadingProgress()];
  }
  [self updateShareMenuForWebState:self.webState];
  if (base::FeatureList::IsEnabled(kThemeColorInTopToolbar)) {
    [self.consumer setPageThemeColor:self.webState->GetThemeColor()];
    [self.consumer
        setUnderPageBackgroundColor:self.webState
                                        ->GetUnderPageBackgroundColor()];
  }
}

/// Updates the consumer with the new forward and back states.
- (void)updateNavigationBackAndForwardStateForWebState:
    (web::WebState*)webState {
  DCHECK(webState);
  const id<ToolbarConsumer> consumer = self.consumer;
  WebNavigationBrowserAgent* navigationBrowserAgent =
      self.navigationBrowserAgent;
  if (navigationBrowserAgent) {
    [consumer setCanGoForward:navigationBrowserAgent->CanGoForward(webState)];
    [consumer setCanGoBack:navigationBrowserAgent->CanGoBack(webState)];
  }
}

/// Updates the Share Menu button of the consumer.
- (void)updateShareMenuForWebState:(web::WebState*)webState {
  if (!self.webState) {
    return;
  }
  const GURL& URL = webState->GetLastCommittedURL();

  // Enable sharing when the current page url is valid and the url is not app
  // specific (the url's scheme is `chrome`) except when:
  // 1. The page url represents a chrome's download path `chrome://downloads`.
  // 2. The page url is a reference to an external file
  //    `chrome://external-file`.
  BOOL shareMenuEnabled =
      URL.is_valid() &&
      (UrlIsDownloadedFile(URL) || UrlIsExternalFileReference(URL) ||
       !web::GetWebClient()->IsAppSpecificURL(URL));
  // Page sharing requires JavaScript execution, which is paused while overlays
  // are displayed over the web content area.
  [self.consumer setShareMenuEnabled:shareMenuEnabled &&
                                     !self.webContentAreaShowingOverlay];
}

#pragma mark - OverlayPresesenterObserving

- (void)overlayPresenter:(OverlayPresenter*)presenter
    willShowOverlayForRequest:(OverlayRequest*)request
          initialPresentation:(BOOL)initialPresentation {
  self.webContentAreaShowingOverlay = YES;
}

- (void)overlayPresenter:(OverlayPresenter*)presenter
    didHideOverlayForRequest:(OverlayRequest*)request {
  self.webContentAreaShowingOverlay = NO;
}

- (void)overlayPresenterDestroyed:(OverlayPresenter*)presenter {
  self.webContentAreaOverlayPresenter = nullptr;
}

#pragma mark - Private

/// Returns a menu for the `navigationItems`.
- (UIMenu*)menuForNavigationItems:
    (const std::vector<web::NavigationItem*>)navigationItems {
  NSMutableArray<UIMenuElement*>* actions = [NSMutableArray array];
  for (web::NavigationItem* navigationItem : navigationItems) {
    NSString* title;
    UIImage* image;
    if ([self shouldUseIncognitoNTPResourcesForURL:navigationItem
                                                       ->GetVirtualURL()]) {
      title = l10n_util::GetNSStringWithFixup(IDS_IOS_NEW_INCOGNITO_TAB);
      image = SymbolWithPalette(
          CustomSymbolWithPointSize(kIncognitoSymbol, kInfobarSymbolPointSize),
          @[ UIColor.whiteColor ]);
    } else {
      title = base::SysUTF16ToNSString(navigationItem->GetTitleForDisplay());
      const gfx::Image& gfxImage = navigationItem->GetFaviconStatus().image;
      if (!gfxImage.IsEmpty()) {
        image = gfxImage.ToUIImage();
      } else {
        image = DefaultSymbolWithPointSize(kDocSymbol, kInfobarSymbolPointSize);
      }
    }

    __weak __typeof(self) weakSelf = self;
    UIAction* action =
        [UIAction actionWithTitle:title
                            image:image
                       identifier:nil
                          handler:^(UIAction* uiAction) {
                            [weakSelf navigateToPageForItem:navigationItem];
                          }];
    [actions addObject:action];
  }
  return [UIMenu menuWithTitle:@"" children:actions];
}

/// Returns YES if incognito NTP title and image should be used for back/forward
/// item associated with `URL`.
- (BOOL)shouldUseIncognitoNTPResourcesForURL:(const GURL&)URL {
  return URL.DeprecatedGetOriginAsURL() == kChromeUINewTabURL &&
         self.isIncognito;
}

/// Returns the menu for the new tab button.
- (UIMenu*)menuForNewTabButton {
  UIAction* voiceSearch = [self.actionFactory actionToStartVoiceSearch];
  UIAction* newSearch = [self.actionFactory actionToStartNewSearch];
  UIAction* newIncognitoSearch =
      [self.actionFactory actionToStartNewIncognitoSearch];
  UIAction* cameraSearch;

  const bool useLens =
      lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
          LensEntrypoint::PlusButton, [self isGoogleDefaultSearchEngine]);
  NSArray* staticActions;
  if (useLens) {
    cameraSearch = [self.actionFactory
        actionToSearchWithLensWithEntryPoint:LensEntrypoint::PlusButton];
  } else {
    cameraSearch = [self.actionFactory actionToShowQRScanner];
  }

  if (experimental_flags::EnableAIPrototypingMenu()) {
    UIAction* openAIMenu = [self.actionFactory actionToOpenAIMenu];
    staticActions = @[
      newSearch, newIncognitoSearch, voiceSearch, cameraSearch, openAIMenu
    ];
  } else {
    staticActions =
        @[ newSearch, newIncognitoSearch, voiceSearch, cameraSearch ];
  }

  UIMenuElement* clipboardAction = [self menuElementForPasteboard];

  if (clipboardAction) {
    UIMenu* staticMenu = [UIMenu menuWithTitle:@""
                                         image:nil
                                    identifier:nil
                                       options:UIMenuOptionsDisplayInline
                                      children:staticActions];

    return [UIMenu menuWithTitle:@"" children:@[ staticMenu, clipboardAction ]];
  }
  return [UIMenu menuWithTitle:@"" children:staticActions];
}

/// Returns the menu for the TabGrid button.
- (UIMenu*)menuForTabGridButton {
  UIAction* openNewTab = [self.actionFactory actionToOpenNewTab];

  UIAction* openNewIncognitoTab =
      [self.actionFactory actionToOpenNewIncognitoTab];

  UIAction* closeTab = [self.actionFactory actionToCloseCurrentTab];

  return [UIMenu menuWithTitle:@""
                      children:@[ closeTab, openNewTab, openNewIncognitoTab ]];
}

/// Returns the UIMenuElement for the content of the pasteboard. Can return nil.
- (UIMenuElement*)menuElementForPasteboard {
  std::optional<std::set<ClipboardContentType>> clipboardContentType =
      ClipboardRecentContent::GetInstance()->GetCachedClipboardContentTypes();

  if (clipboardContentType.has_value()) {
    std::set<ClipboardContentType> clipboardContentTypeValues =
        clipboardContentType.value();

    if (search_engines::SupportsSearchByImage(self.templateURLService) &&
        base::Contains(clipboardContentTypeValues,
                       ClipboardContentType::Image)) {
      return [self.actionFactory actionToSearchCopiedImage];
    } else if (base::Contains(clipboardContentTypeValues,
                              ClipboardContentType::URL)) {
      return [self.actionFactory actionToSearchCopiedURL];
    } else if (base::Contains(clipboardContentTypeValues,
                              ClipboardContentType::Text)) {
      return [self.actionFactory actionToSearchCopiedText];
    }
  }
  return nil;
}

/// Navigates to the page associated with `item`.
- (void)navigateToPageForItem:(web::NavigationItem*)item {
  if (!self.webState) {
    return;
  }

  int index = self.webState->GetNavigationManager()->GetIndexOfItem(item);
  DCHECK_NE(index, -1);
  self.webState->GetNavigationManager()->GoToIndex(index);
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

// Returns the tab group of the active web state, if any.
- (const TabGroup*)activeWebStateTabGroup {
  if (IsTabGroupInGridEnabled()) {
    const int active_index = _webStateList->active_index();
    if (active_index != WebStateList::kInvalidIndex) {
      return _webStateList->GetGroupOfWebStateAt(active_index);
    }
  }
  return nullptr;
}

// Returns the tab count to display in the Tab Grid button.
- (int)tabCountToDisplay {
  const TabGroup* activeTabGroup = [self activeWebStateTabGroup];
  if (activeTabGroup == nullptr) {
    return _webStateList->count();
  }

  return IsTabGroupIndicatorEnabled() && HasTabGroupIndicatorButtonsUpdated()
             ? activeTabGroup->range().count()
             : _webStateList->count();
}

// Returns the tab group state to display in the Tab Grid button.
- (ToolbarTabGroupState)tabGroupStateToDisplay {
  const TabGroup* activeTabGroup = [self activeWebStateTabGroup];
  if (activeTabGroup == nullptr) {
    return ToolbarTabGroupState::kNormal;
  }

  return IsTabGroupIndicatorEnabled() && HasTabGroupIndicatorButtonsUpdated()
             ? ToolbarTabGroupState::kTabGroup
             : ToolbarTabGroupState::kNormal;
}

// Updates the blue dot in the Tab Grid button depending on the messages and the
// current active web state.
- (void)updateTabGridButtonBlueDot {
  if ([self tabGroupStateToDisplay] == ToolbarTabGroupState::kNormal) {
    // Show the blue dot if there is at least one group that has been updated.
    [self.consumer setTabGridButtonBlueDot:_dirtyGroups.size() > 0];
    return;
  }

  // Show the blue dot if the current active group has been updated.
  CHECK([self tabGroupStateToDisplay] == ToolbarTabGroupState::kTabGroup);
  const TabGroup* activeGroup = [self activeWebStateTabGroup];
  [self.consumer setTabGridButtonBlueDot:_dirtyGroups.contains(
                                             activeGroup->tab_group_id())];
}

// Gets messages to indicate that a shared tab group has been changed.
- (void)fetchMessages {
  if (!_messagingService || !_messagingService->IsInitialized() ||
      !_webStateList) {
    return;
  }

  std::vector<collaboration::messaging::PersistentMessage> messages =
      _messagingService->GetMessages(
          collaboration::messaging::PersistentNotificationType::
              DIRTY_TAB_GROUP);

  for (auto& message : messages) {
    if (std::optional<tab_groups::LocalTabGroupID> localTabGroupID =
            LocalTabGroupID(message)) {
      _dirtyGroups.insert(*localTabGroupID);
    }
  }

  [self updateTabGridButtonBlueDot];
}

@end
