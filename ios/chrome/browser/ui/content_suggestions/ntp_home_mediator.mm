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
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
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
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_metrics.h"
#import "ios/chrome/browser/ui/location_bar_notification_names.h"
#include "ios/chrome/browser/ui/ntp/metrics.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/common/favicon/favicon_attributes.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
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

@interface NTPHomeMediator ()<CRWWebStateObserver,
                              SearchEngineObserving,
                              WebStateListObserving> {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  // Observes the WebStateList so that this mediator can update the UI when the
  // active WebState changes.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  // Listen for default search engine changes.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
}

@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
// The WebStateList that is being observed by this mediator.
@property(nonatomic, assign, readonly) WebStateList* webStateList;
// TemplateURL used to get the search engine.
@property(nonatomic, assign) TemplateURLService* templateURLService;
// Logo vendor to display the doodle on the NTP.
@property(nonatomic, strong) id<LogoVendor> logoVendor;
// The web state associated with this NTP.
@property(nonatomic, assign) web::WebState* webState;

@end

@implementation NTPHomeMediator

@synthesize webState = _webState;
@synthesize consumer = _consumer;
@synthesize dispatcher = _dispatcher;
@synthesize suggestionsService = _suggestionsService;
@synthesize NTPMetrics = _NTPMetrics;
@synthesize suggestionsViewController = _suggestionsViewController;
@synthesize suggestionsMediator = _suggestionsMediator;
@synthesize alertCoordinator = _alertCoordinator;
@synthesize metricsRecorder = _metricsRecorder;
@synthesize logoVendor = _logoVendor;
@synthesize templateURLService = _templateURLService;
@synthesize webStateList = _webStateList;

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                  templateURLService:(TemplateURLService*)templateURLService
                          logoVendor:(id<LogoVendor>)logoVendor {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
    _templateURLService = templateURLService;
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

  [self.consumer setTabCount:self.webStateList->count()];
  self.webState = self.webStateList->GetActiveWebState();

  _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
  if (self.webState) {
    self.webState->AddObserver(_webStateObserver.get());
    web::NavigationManager* navigationManager =
        self.webState->GetNavigationManager();
    [self.consumer setCanGoForward:navigationManager->CanGoForward()];
    [self.consumer setCanGoBack:navigationManager->CanGoBack()];
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
  _webStateList->RemoveObserver(_webStateListObserver.get());
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

  web::NavigationManager::WebLoadParams params(suggestionItem.URL);
  // Use a referrer with a specific URL to mark this entry as coming from
  // ContentSuggestions.
  params.referrer =
      web::Referrer(GURL(ntp_snippets::GetContentSuggestionsReferrerURL()),
                    web::ReferrerPolicyDefault);
  params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  [self.dispatcher loadURLWithParams:params];
  [self.NTPMetrics recordAction:new_tab_page_uma::ACTION_OPENED_SUGGESTION];
}

- (void)openMostVisitedItem:(CollectionViewItem*)item
                    atIndex:(NSInteger)mostVisitedIndex {
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
    }
    return;
  }

  ContentSuggestionsMostVisitedItem* mostVisitedItem =
      base::mac::ObjCCastStrict<ContentSuggestionsMostVisitedItem>(item);

  [self logMostVisitedOpening:mostVisitedItem atIndex:mostVisitedIndex];

  web::NavigationManager::WebLoadParams params(mostVisitedItem.URL);
  params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  [self.dispatcher loadURLWithParams:params];
}

- (void)displayContextMenuForSuggestion:(CollectionViewItem*)item
                                atPoint:(CGPoint)touchLocation
                            atIndexPath:(NSIndexPath*)indexPath
                        readLaterAction:(BOOL)readLaterAction {
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
    OpenNewTabCommand* command =
        [[OpenNewTabCommand alloc] initWithURL:notificationPromo->url()
                                      referrer:web::Referrer()
                                   inIncognito:NO
                                  inBackground:NO
                                      appendTo:kCurrentTab];
    [self.dispatcher webPageOrderedOpen:command];
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
  GURL URL(kNTPHelpURL);
  web::NavigationManager::WebLoadParams params(URL);
  [self.dispatcher loadURLWithParams:params];
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

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [self.consumer setTabCount:self.webStateList->count()];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  [self.consumer setTabCount:self.webStateList->count()];
}

// If the actual webState associated with this mediator were passed in, this
// would not be necessary.  However, since the active webstate can change when
// the new tab page is created (and animated in), listen for changes here and
// always display what's active.
- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  if (newWebState) {
    self.webState = newWebState;
    web::NavigationManager* navigationManager =
        newWebState->GetNavigationManager();
    [self.consumer setCanGoForward:navigationManager->CanGoForward()];
    [self.consumer setCanGoBack:navigationManager->CanGoBack()];
  }
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
  OpenNewTabCommand* command =
      [[OpenNewTabCommand alloc] initWithURL:URL
                                    referrer:web::Referrer()
                                 inIncognito:incognito
                                inBackground:!incognito
                                    appendTo:kCurrentTab];
  command.originPoint = originPoint;
  [self.dispatcher webPageOrderedOpen:command];
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

  if (!base::FeatureList::IsEnabled(kBrowserContainerContainsNTP))
    return;

  web::NavigationManager* manager = webState->GetNavigationManager();
  web::NavigationItem* item = manager->GetLastCommittedItem();
  web::PageDisplayState displayState;
  CGPoint scrollOffset =
      self.suggestionsViewController.collectionView.contentOffset;
  scrollOffset.y -=
      self.headerCollectionInteractionHandler.collectionShiftingOffset;
  displayState.scroll_state().set_offset_x(scrollOffset.x);
  displayState.scroll_state().set_offset_y(scrollOffset.y);
  item->SetPageDisplayState(displayState);
}

// Set the NTP scroll offset for the current navigation item.
- (void)setContentOffsetForWebState:(web::WebState*)webState {
  if (webState->GetVisibleURL().GetOrigin() != kChromeUINewTabURL) {
    return;
  }
  web::NavigationManager* navigationManager = webState->GetNavigationManager();
  web::NavigationItem* item = navigationManager->GetVisibleItem();
  if (item && item->GetPageDisplayState().scroll_state().offset_y() > 0) {
    CGFloat offset = item->GetPageDisplayState().scroll_state().offset_y();
    [self.suggestionsViewController setContentOffset:offset];
  }
}

@end
