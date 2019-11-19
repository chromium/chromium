// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_mediator.h"

#include <memory>

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/features.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_alert_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_metrics.h"
#import "ios/chrome/browser/ui/content_suggestions/user_account_image_update_delegate.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_notification_names.h"
#include "ios/chrome/browser/ui/ntp/metrics.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/url_loading/url_loading_service.h"
#import "ios/chrome/common/favicon/favicon_attributes.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// URL for the page displaying help for the NTP.
const char kNTPHelpURL[] =
    "https://support.google.com/chrome/?p=ios_new_tab&ios=1";
}  // namespace

@interface NTPHomeMediator () <CRWWebStateObserver,
                               IdentityManagerObserverBridgeDelegate,
                               SearchEngineObserving> {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  // Listen for default search engine changes.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  // Observes changes in identity and updates the Identity Disc.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
  // Used to load URLs.
  UrlLoadingService* _urlLoadingService;
}

@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
// TemplateURL used to get the search engine.
@property(nonatomic, assign) TemplateURLService* templateURLService;
// Authentication Service to get the current user's avatar.
@property(nonatomic, assign) AuthenticationService* authService;
// Logo vendor to display the doodle on the NTP.
@property(nonatomic, strong) id<LogoVendor> logoVendor;
// The web state associated with this NTP.
@property(nonatomic, assign) web::WebState* webState;
// This is the object that knows how to update the Identity Disc UI.
@property(nonatomic, weak) id<UserAccountImageUpdateDelegate> imageUpdater;

@end

@implementation NTPHomeMediator

- (instancetype)initWithWebState:(web::WebState*)webState
              templateURLService:(TemplateURLService*)templateURLService
               urlLoadingService:(UrlLoadingService*)urlLoadingService
                     authService:(AuthenticationService*)authService
                 identityManager:(signin::IdentityManager*)identityManager
                      logoVendor:(id<LogoVendor>)logoVendor {
  self = [super init];
  if (self) {
    _webState = webState;
    _templateURLService = templateURLService;
    _urlLoadingService = urlLoadingService;
    _authService = authService;
    _identityObserverBridge.reset(
        new signin::IdentityManagerObserverBridge(identityManager, self));
    // Listen for default search engine changes.
    _searchEngineObserver = std::make_unique<SearchEngineObserverBridge>(
        self, self.templateURLService);
    _logoVendor = logoVendor;
  }
  return self;
}

- (void)dealloc {
  if (_webState && _webStateObserver) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    _webState = nullptr;
  }
}

- (void)setUp {
  DCHECK(!_webStateObserver);
  DCHECK(self.suggestionsService);

  _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
  if (self.webState) {
    self.webState->AddObserver(_webStateObserver.get());
  }

  [self.consumer setLogoVendor:self.logoVendor];

  self.templateURLService->Load();
  [self searchEngineChanged];

  // Set up notifications;
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self.consumer
                    selector:@selector(locationBarBecomesFirstResponder)
                        name:kLocationBarBecomesFirstResponderNotification
                      object:nil];
  [defaultCenter addObserver:self.consumer
                    selector:@selector(locationBarResignsFirstResponder)
                        name:kLocationBarResignsFirstResponderNotification
                      object:nil];
}

- (void)shutdown {
  [[NSNotificationCenter defaultCenter] removeObserver:self.consumer];
  _searchEngineObserver.reset();
  DCHECK(_webStateObserver);
  if (_webState) {
    [self saveContentOffsetForWebState:_webState];
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
  }
}

#pragma mark - Properties.

- (void)setWebState:(web::WebState*)webState {
  if (_webState && _webStateObserver) {
    _webState->RemoveObserver(_webStateObserver.get());
  }
  _webState = webState;
  if (_webState && _webStateObserver) {
    _webState->AddObserver(_webStateObserver.get());
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self setContentOffsetForWebState:webState];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(self.webState, webState);
  self.webState = nullptr;
}

#pragma mark - ContentSuggestionsCommands

- (void)openReadingList {
  [self.dispatcher showReadingList];
}

- (void)openPageForItemAtIndexPath:(NSIndexPath*)indexPath {
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(self.webState);
  if (NTPHelper && NTPHelper->IgnoreLoadRequests()) {
    return;
  }
  CollectionViewItem* item = [self.suggestionsViewController.collectionViewModel
      itemAtIndexPath:indexPath];
  ContentSuggestionsItem* suggestionItem =
      base::mac::ObjCCastStrict<ContentSuggestionsItem>(item);

  [self.metricsRecorder
         onSuggestionOpened:suggestionItem
                atIndexPath:indexPath
         sectionsShownAbove:[self.suggestionsViewController
                                numberOfSectionsAbove:indexPath.section]
      suggestionsShownAbove:[self.suggestionsViewController
                                numberOfSuggestionsAbove:indexPath.section]
                 withAction:WindowOpenDisposition::CURRENT_TAB];
  self.suggestionsService->user_classifier()->OnEvent(
      ntp_snippets::UserClassifier::Metric::SUGGESTIONS_USED);

  // Use a referrer with a specific URL to mark this entry as coming from
  // ContentSuggestions.
  UrlLoadParams params = UrlLoadParams::InCurrentTab(suggestionItem.URL);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  params.web_params.referrer =
      web::Referrer(GURL(ntp_snippets::GetContentSuggestionsReferrerURL()),
                    web::ReferrerPolicyDefault);
  _urlLoadingService->Load(params);
  [self.NTPMetrics recordAction:new_tab_page_uma::ACTION_OPENED_SUGGESTION];
}

- (void)openMostVisitedItem:(CollectionViewItem*)item
                    atIndex:(NSInteger)mostVisitedIndex {
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(self.webState);
  if (NTPHelper && NTPHelper->IgnoreLoadRequests())
    return;

  if ([item isKindOfClass:[ContentSuggestionsMostVisitedActionItem class]]) {
    ContentSuggestionsMostVisitedActionItem* mostVisitedItem =
        base::mac::ObjCCastStrict<ContentSuggestionsMostVisitedActionItem>(
            item);
    switch (mostVisitedItem.collectionShortcutType) {
      case NTPCollectionShortcutTypeBookmark:
        [self.dispatcher showBookmarksManager];
        base::RecordAction(base::UserMetricsAction("MobileNTPShowBookmarks"));
        break;
      case NTPCollectionShortcutTypeReadingList:
        [self.dispatcher showReadingList];
        base::RecordAction(base::UserMetricsAction("MobileNTPShowReadingList"));
        break;
      case NTPCollectionShortcutTypeRecentTabs:
        [self.dispatcher showRecentTabs];
        base::RecordAction(base::UserMetricsAction("MobileNTPShowRecentTabs"));
        break;
      case NTPCollectionShortcutTypeHistory:
        [self.dispatcher showHistory];
        base::RecordAction(base::UserMetricsAction("MobileNTPShowHistory"));
        break;
      case NTPCollectionShortcutTypeCount:
        NOTREACHED();
        break;
    }
    return;
  }

  ContentSuggestionsMostVisitedItem* mostVisitedItem =
      base::mac::ObjCCastStrict<ContentSuggestionsMostVisitedItem>(item);

  [self logMostVisitedOpening:mostVisitedItem atIndex:mostVisitedIndex];

  UrlLoadParams params = UrlLoadParams::InCurrentTab(mostVisitedItem.URL);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  _urlLoadingService->Load(params);
}

- (void)displayContextMenuForSuggestion:(CollectionViewItem*)item
                                atPoint:(CGPoint)touchLocation
                            atIndexPath:(NSIndexPath*)indexPath
                        readLaterAction:(BOOL)readLaterAction {
  // Unfocus the omnibox as the omnibox can disappear when choosing some
  // options. See crbug.com/928237.
  [self.dispatcher cancelOmniboxEdit];

  ContentSuggestionsItem* suggestionsItem =
      base::mac::ObjCCastStrict<ContentSuggestionsItem>(item);

  [self.metricsRecorder
      onMenuOpenedForSuggestion:suggestionsItem
                    atIndexPath:indexPath
          suggestionsShownAbove:[self.suggestionsViewController
                                    numberOfSuggestionsAbove:indexPath
                                                                 .section]];

  self.alertCoordinator = [ContentSuggestionsAlertFactory
      alertCoordinatorForSuggestionItem:suggestionsItem
                       onViewController:self.suggestionsViewController
                                atPoint:touchLocation
                            atIndexPath:indexPath
                        readLaterAction:readLaterAction
                         commandHandler:self];

  [self.alertCoordinator start];
}

- (void)displayContextMenuForMostVisitedItem:(CollectionViewItem*)item
                                     atPoint:(CGPoint)touchLocation
                                 atIndexPath:(NSIndexPath*)indexPath {
  // No context menu for action buttons.
  if ([item isKindOfClass:[ContentSuggestionsMostVisitedActionItem class]]) {
    return;
  }

  // Unfocus the omnibox as the omnibox can disappear when choosing some
  // options. See crbug.com/928237.
  [self.dispatcher cancelOmniboxEdit];

  ContentSuggestionsMostVisitedItem* mostVisitedItem =
      base::mac::ObjCCastStrict<ContentSuggestionsMostVisitedItem>(item);
  self.alertCoordinator = [ContentSuggestionsAlertFactory
      alertCoordinatorForMostVisitedItem:mostVisitedItem
                        onViewController:self.suggestionsViewController
                                 atPoint:touchLocation
                             atIndexPath:indexPath
                          commandHandler:self];

  [self.alertCoordinator start];
}

- (void)dismissModals {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

// TODO(crbug.com/761096) : Promo handling should be tested.
- (void)handlePromoTapped {
  NotificationPromoWhatsNew* notificationPromo =
      [self.suggestionsMediator notificationPromo];
  DCHECK(notificationPromo);
  notificationPromo->HandleClosed();
  [self.NTPMetrics recordAction:new_tab_page_uma::ACTION_OPENED_PROMO];

  if (notificationPromo->IsURLPromo()) {
    UrlLoadParams params = UrlLoadParams::InNewTab(notificationPromo->url());
    params.append_to = kCurrentTab;
    _urlLoadingService->Load(params);
    return;
  }

  if (notificationPromo->IsChromeCommandPromo()) {
    // "What's New" promo that runs a command can be added here by calling
    // self.dispatcher.
    DCHECK_EQ(kTestWhatsNewCommand, notificationPromo->command())
        << "Promo command is not valid.";
    return;
  }
  NOTREACHED() << "Promo type is neither URL or command.";
}

- (void)handleLearnMoreTapped {
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(self.webState);
  if (NTPHelper && NTPHelper->IgnoreLoadRequests())
    return;
  _urlLoadingService->Load(UrlLoadParams::InCurrentTab(GURL(kNTPHelpURL)));
  [self.NTPMetrics recordAction:new_tab_page_uma::ACTION_OPENED_LEARN_MORE];
}

#pragma mark - ContentSuggestionsGestureCommands

- (void)openNewTabWithSuggestionsItem:(ContentSuggestionsItem*)item
                            incognito:(BOOL)incognito {
  [self.NTPMetrics recordAction:new_tab_page_uma::ACTION_OPENED_SUGGESTION];
  self.suggestionsService->user_classifier()->OnEvent(
      ntp_snippets::UserClassifier::Metric::SUGGESTIONS_USED);

  NSIndexPath* indexPath = [self.suggestionsViewController.collectionViewModel
      indexPathForItem:item];
  if (indexPath) {
    WindowOpenDisposition disposition =
        incognito ? WindowOpenDisposition::OFF_THE_RECORD
                  : WindowOpenDisposition::NEW_BACKGROUND_TAB;
    [self.metricsRecorder
           onSuggestionOpened:item
                  atIndexPath:indexPath
           sectionsShownAbove:[self.suggestionsViewController
                                  numberOfSectionsAbove:indexPath.section]
        suggestionsShownAbove:[self.suggestionsViewController
                                  numberOfSuggestionsAbove:indexPath.section]
                   withAction:disposition];
  }

  CGPoint cellCenter = [self cellCenterForItem:item];
  [self openNewTabWithURL:item.URL incognito:incognito originPoint:cellCenter];
}

- (void)addItemToReadingList:(ContentSuggestionsItem*)item {
  NSIndexPath* indexPath = [self.suggestionsViewController.collectionViewModel
      indexPathForItem:item];
  if (indexPath) {
    [self.metricsRecorder
           onSuggestionOpened:item
                  atIndexPath:indexPath
           sectionsShownAbove:[self.suggestionsViewController
                                  numberOfSectionsAbove:indexPath.section]
        suggestionsShownAbove:[self.suggestionsViewController
                                  numberOfSuggestionsAbove:indexPath.section]
                   withAction:WindowOpenDisposition::SAVE_TO_DISK];
  }

  self.suggestionsMediator.readingListNeedsReload = YES;
  ReadingListAddCommand* command =
      [[ReadingListAddCommand alloc] initWithURL:item.URL title:item.title];
  [self.dispatcher addToReadingList:command];
}

- (void)dismissSuggestion:(ContentSuggestionsItem*)item
              atIndexPath:(NSIndexPath*)indexPath {
  NSIndexPath* itemIndexPath = indexPath;
  if (!itemIndexPath) {
    // If the caller uses a nil |indexPath|, find it from the model.
    itemIndexPath = [self.suggestionsViewController.collectionViewModel
        indexPathForItem:item];
  }

  [self.suggestionsMediator dismissSuggestion:item.suggestionIdentifier];
  [self.suggestionsViewController dismissEntryAtIndexPath:itemIndexPath];
}

- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                            incognito:(BOOL)incognito
                              atIndex:(NSInteger)index {
  [self logMostVisitedOpening:item atIndex:index];
  CGPoint cellCenter = [self cellCenterForItem:item];
  [self openNewTabWithURL:item.URL incognito:incognito originPoint:cellCenter];
}

- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                            incognito:(BOOL)incognito {
  NSInteger index =
      [self.suggestionsViewController.collectionViewModel indexPathForItem:item]
          .item;
  [self openNewTabWithMostVisitedItem:item incognito:incognito atIndex:index];
}

- (void)removeMostVisited:(ContentSuggestionsMostVisitedItem*)item {
  base::RecordAction(base::UserMetricsAction("MostVisited_UrlBlacklisted"));
  [self.suggestionsMediator blacklistMostVisitedURL:item.URL];
  [self showMostVisitedUndoForURL:item.URL];
}

#pragma mark - ContentSuggestionsHeaderViewControllerDelegate

- (BOOL)isContextMenuVisible {
  return self.alertCoordinator.isVisible;
}

- (BOOL)isScrolledToTop {
  return self.suggestionsViewController.scrolledToTop;
}

- (void)registerImageUpdater:(id<UserAccountImageUpdateDelegate>)imageUpdater {
  self.imageUpdater = imageUpdater;
  [self updateAccountImage];
}

- (BOOL)ignoreLoadRequests {
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(self.webState);
  if (NTPHelper && NTPHelper->IgnoreLoadRequests()) {
    return YES;
  }
  return NO;
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  BOOL showLogo = NO;
  const TemplateURL* defaultURL =
      self.templateURLService->GetDefaultSearchProvider();
  if (defaultURL) {
    showLogo = defaultURL->GetEngineType(
                   self.templateURLService->search_terms_data()) ==
               SEARCH_ENGINE_GOOGLE;
  }
  [self.consumer setLogoIsShowing:showLogo];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountSet:(const CoreAccountInfo&)primaryAccountInfo {
  [self updateAccountImage];
}

- (void)onPrimaryAccountCleared:
    (const CoreAccountInfo&)previousPrimaryAccountInfo {
  [self updateAccountImage];
}

#pragma mark - Private

// Returns the center of the cell associated with |item| in the window
// coordinates. Returns CGPointZero is the cell isn't visible.
- (CGPoint)cellCenterForItem:(CollectionViewItem<SuggestedContent>*)item {
  NSIndexPath* indexPath = [self.suggestionsViewController.collectionViewModel
      indexPathForItem:item];
  if (!indexPath)
    return CGPointZero;

  UIView* cell = [self.suggestionsViewController.collectionView
      cellForItemAtIndexPath:indexPath];
  return [cell.superview convertPoint:cell.center toView:nil];
}

// Opens the |URL| in a new tab |incognito| or not. |originPoint| is the origin
// of the new tab animation if the tab is opened in background, in window
// coordinates.
- (void)openNewTabWithURL:(const GURL&)URL
                incognito:(BOOL)incognito
              originPoint:(CGPoint)originPoint {
  // Open the tab in background if it is non-incognito only.
  UrlLoadParams params = UrlLoadParams::InNewTab(URL);
  params.SetInBackground(!incognito);
  params.in_incognito = incognito;
  params.append_to = kCurrentTab;
  params.origin_point = originPoint;
  _urlLoadingService->Load(params);
}

// Logs a histogram due to a Most Visited item being opened.
- (void)logMostVisitedOpening:(ContentSuggestionsMostVisitedItem*)item
                      atIndex:(NSInteger)mostVisitedIndex {
  [self.NTPMetrics
      recordAction:new_tab_page_uma::ACTION_OPENED_MOST_VISITED_ENTRY];
  base::RecordAction(base::UserMetricsAction("MobileNTPMostVisited"));

  // TODO(crbug.com/763946): Plumb generation time.
  RecordNTPTileClick(mostVisitedIndex, item.source, item.titleSource,
                     item.attributes, base::Time(), GURL());
}

// Shows a snackbar with an action to undo the removal of the most visited item
// with a |URL|.
- (void)showMostVisitedUndoForURL:(GURL)URL {
  GURL copiedURL = URL;

  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  __weak NTPHomeMediator* weakSelf = self;
  action.handler = ^{
    NTPHomeMediator* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [strongSelf.suggestionsMediator whitelistMostVisitedURL:copiedURL];
  };
  action.title = l10n_util::GetNSString(IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE);
  action.accessibilityIdentifier = @"Undo";

  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = [MDCSnackbarMessage
      messageWithText:l10n_util::GetNSString(
                          IDS_IOS_NEW_TAB_MOST_VISITED_ITEM_REMOVED)];
  message.action = action;
  message.category = @"MostVisitedUndo";
  [self.dispatcher showSnackbarMessage:message];
}

// Save the NTP scroll offset into the last committed navigation item for the
// before we navigate away.
- (void)saveContentOffsetForWebState:(web::WebState*)webState {
  if (webState->GetLastCommittedURL().GetOrigin() != kChromeUINewTabURL)
    return;

  web::NavigationManager* manager = webState->GetNavigationManager();
  web::NavigationItem* item = manager->GetLastCommittedItem();
  web::PageDisplayState displayState;
  UIEdgeInsets contentInset =
      self.suggestionsViewController.collectionView.contentInset;
  CGPoint contentOffset =
      self.suggestionsViewController.collectionView.contentOffset;
  contentOffset.y -=
      self.headerCollectionInteractionHandler.collectionShiftingOffset;
  displayState.scroll_state() =
      web::PageScrollState(contentOffset, contentInset);
  item->SetPageDisplayState(displayState);
}

// Set the NTP scroll offset for the current navigation item.
- (void)setContentOffsetForWebState:(web::WebState*)webState {
  if (webState->GetVisibleURL().GetOrigin() != kChromeUINewTabURL) {
    return;
  }
  web::NavigationManager* navigationManager = webState->GetNavigationManager();
  web::NavigationItem* item = navigationManager->GetVisibleItem();
  CGFloat offset =
      item ? item->GetPageDisplayState().scroll_state().content_offset().y : 0;
  if (offset > 0)
    [self.suggestionsViewController setContentOffset:offset];
}

// Fetches and update user's avatar on NTP, or use default avatar if user is
// not signed in.
- (void)updateAccountImage {
  UIImage* image;
  // Fetches user's identity from Authentication Service.
  ChromeIdentity* identity = self.authService->GetAuthenticatedIdentity();
  if (identity) {
    // Fetches user's avatar from Authentication Service. Use cached version if
    // one is available. If not, use the default avatar and initiate a fetch
    // in the background. When background fetch completes, all observers will
    // be notified to refresh the user's avatar.
    ios::ChromeIdentityService* identityService =
        ios::GetChromeBrowserProvider()->GetChromeIdentityService();
    image = identityService->GetCachedAvatarForIdentity(identity);
    if (!image) {
      image = [self defaultAvatar];
      identityService->GetAvatarForIdentity(identity, nil);
    }
  } else {
    // User is not signed in, show default avatar.
    image = [self defaultAvatar];
  }
  // TODO(crbug.com/965962): Use ResizedAvatarCache when it accepts the
  // specification of different image sizes.
  CGFloat dimension = ntp_home::kIdentityAvatarDimension;
  if (image.size.width != dimension || image.size.height != dimension) {
    image = ResizeImage(image, CGSizeMake(dimension, dimension),
                        ProjectionMode::kAspectFit);
  }
  [self.imageUpdater updateAccountImage:image];
}

// Returns the default avatar image for users who are not signed in or signed
// in but avatar image is not available yet.
- (UIImage*)defaultAvatar {
  return ios::GetChromeBrowserProvider()
      ->GetSigninResourcesProvider()
      ->GetDefaultAvatar();
}

@end
