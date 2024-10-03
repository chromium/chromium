// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper_observer_bridge.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/translate/core/browser/translate_manager.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/find_in_page/model/abstract_find_tab_helper.h"
#import "ios/chrome/browser/follow/model/follow_browser_agent.h"
#import "ios/chrome/browser/follow/model/follow_menu_updater.h"
#import "ios/chrome/browser/follow/model/follow_tab_helper.h"
#import "ios/chrome/browser/follow/model/follow_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer_bridge.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"
#import "ios/chrome/browser/shared/public/commands/search_image_with_lens_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_constants.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_text_item.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_tools_item.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/public/cells/popup_menu_item.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_consumer.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_menu_notification_delegate.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_menu_notifier.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_utils.h"
#import "ios/chrome/browser/url_loading/model/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web/model/font_size/font_size_tab_helper.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ios/web/common/features.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/favicon/favicon_status.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/image/image.h"

using base::RecordAction;
using base::UserMetricsAction;

namespace {
PopupMenuToolsItem* CreateTableViewItem(int titleID,
                                        PopupMenuAction action,
                                        NSString* imageName,
                                        NSString* accessibilityID) {
  PopupMenuToolsItem* item =
      [[PopupMenuToolsItem alloc] initWithType:kItemTypeEnumZero];
  item.title = l10n_util::GetNSString(titleID);
  item.actionIdentifier = action;
  item.accessibilityIdentifier = accessibilityID;
  if (imageName) {
    item.image = [[UIImage imageNamed:imageName]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }
  return item;
}

PopupMenuToolsItem* CreateFollowItem(int titleID,
                                     PopupMenuAction action,
                                     NSString* imageName,
                                     NSString* accessibilityID) {
  DCHECK(IsWebChannelsEnabled());
  PopupMenuToolsItem* item =
      [[PopupMenuToolsItem alloc] initWithType:kItemTypeEnumZero];
  item.title = l10n_util::GetNSStringF(titleID, base::SysNSStringToUTF16(@""));
  item.enabled = NO;
  item.actionIdentifier = action;
  item.accessibilityIdentifier = accessibilityID;
  if (imageName) {
    item.image = [[UIImage imageNamed:imageName]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }
  return item;
}

PopupMenuTextItem* CreateEnterpriseInfoItem(NSString* imageName,
                                            NSString* message,
                                            PopupMenuAction action,
                                            NSString* accessibilityID) {
  PopupMenuTextItem* item =
      [[PopupMenuTextItem alloc] initWithType:kItemTypeEnumZero];
  item.imageName = imageName;
  item.message = message;
  item.actionIdentifier = action;
  item.accessibilityIdentifier = accessibilityID;

  return item;
}

}  // namespace

@interface PopupMenuMediator () <BookmarkModelBridgeObserver,
                                 CRWWebStateObserver,
                                 FollowMenuUpdater,
                                 IOSLanguageDetectionTabHelperObserving,
                                 OverlayPresenterObserving,
                                 PrefObserverDelegate,
                                 ReadingListMenuNotificationDelegate,
                                 WebStateListObserving> {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  // Bridge to register for bookmark changes.
  std::unique_ptr<BookmarkModelBridge> _bookmarkModelBridge;
  // Bridge to get notified of the language detection event.
  std::unique_ptr<language::IOSLanguageDetectionTabHelperObserverBridge>
      _iOSLanguageDetectionTabHelperObserverBridge;
  std::unique_ptr<OverlayPresenterObserver> _overlayPresenterObserver;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
}

// Items to be displayed in the popup menu.
@property(nonatomic, strong)
    NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>* items;

// The current web state associated with the toolbar.
@property(nonatomic, assign) web::WebState* webState;

// Whether the popup menu is presented in incognito or not.
@property(nonatomic, assign) BOOL isIncognito;

// Items notifying this items of changes happening to the ReadingList model.
@property(nonatomic, strong) ReadingListMenuNotifier* readingListMenuNotifier;
// The current browser policy connector.
@property(nonatomic, assign) BrowserPolicyConnectorIOS* browserPolicyConnector;

// Whether an overlay is currently presented over the web content area.
@property(nonatomic, assign, getter=isWebContentAreaShowingOverlay)
    BOOL webContentAreaShowingOverlay;

// Whether the web content is currently being blocked.
@property(nonatomic, assign) BOOL contentBlocked;

// URLs for the current webpage, which are used to update the follow status.
@property(nonatomic, strong) WebPageURLs* webPage;

// YES if the current website has been followed.
@property(nonatomic, assign) BOOL followed;

// State of reading list model loading.
@property(nonatomic, assign) BOOL readingListModelLoaded;

#pragma mark*** Specific Items ***

@property(nonatomic, strong) PopupMenuToolsItem* openNewIncognitoTabItem;
@property(nonatomic, strong) PopupMenuToolsItem* reloadStopItem;
@property(nonatomic, strong) PopupMenuToolsItem* followItem;
@property(nonatomic, strong) PopupMenuToolsItem* readLaterItem;
@property(nonatomic, strong) PopupMenuToolsItem* bookmarkItem;
@property(nonatomic, strong) PopupMenuToolsItem* translateItem;
@property(nonatomic, strong) PopupMenuToolsItem* findInPageItem;
@property(nonatomic, strong) PopupMenuToolsItem* textZoomItem;
@property(nonatomic, strong) PopupMenuToolsItem* siteInformationItem;
@property(nonatomic, strong) PopupMenuToolsItem* requestDesktopSiteItem;
@property(nonatomic, strong) PopupMenuToolsItem* requestMobileSiteItem;
@property(nonatomic, strong) PopupMenuToolsItem* readingListItem;
@property(nonatomic, strong) PopupMenuToolsItem* priceNotificationsItem;
// Array containing all the nonnull items/
@property(nonatomic, strong)
    NSArray<TableViewItem<PopupMenuItem>*>* specificItems;

@end

@implementation PopupMenuMediator

#pragma mark - Public

- (instancetype)initWithIsIncognito:(BOOL)isIncognito
                   readingListModel:(ReadingListModel*)readingListModel
             browserPolicyConnector:
                 (BrowserPolicyConnectorIOS*)browserPolicyConnector {
  self = [super init];
  if (self) {
    _isIncognito = isIncognito;
    _readingListMenuNotifier =
        [[ReadingListMenuNotifier alloc] initWithReadingList:readingListModel];
    _readingListModelLoaded = readingListModel->loaded();
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _overlayPresenterObserver =
        std::make_unique<OverlayPresenterObserverBridge>(self);
    _browserPolicyConnector = browserPolicyConnector;
  }
  return self;
}

- (void)disconnect {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }

  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    if (self.followItem) {
      FollowTabHelper* followTabHelper =
          FollowTabHelper::FromWebState(_webState);
      if (followTabHelper) {
        followTabHelper->RemoveFollowMenuUpdater();
      }
      self.webPage = nil;
    }
    _webState = nullptr;
  }

  if (_engagementTracker) {
    if (_readingListItem.badgeText.length != 0) {
      _engagementTracker->Dismissed(
          feature_engagement::kIPHBadgedReadingListFeature);
    }

    if (_translateItem.badgeText.length != 0) {
      _engagementTracker->Dismissed(
          feature_engagement::kIPHBadgedTranslateManualTriggerFeature);
    }

    _engagementTracker = nullptr;
  }

  if (_webContentAreaOverlayPresenter) {
    _webContentAreaOverlayPresenter->RemoveObserver(
        _overlayPresenterObserver.get());
    self.webContentAreaShowingOverlay = NO;
    _webContentAreaOverlayPresenter = nullptr;
  }

  _readingListMenuNotifier = nil;
  _bookmarkModelBridge.reset();
  _iOSLanguageDetectionTabHelperObserverBridge.reset();

  _prefChangeRegistrar.reset();
  _prefObserverBridge.reset();
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webStateDidChangeBackForwardState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  self.webState = nullptr;
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (status.active_web_state_change()) {
    self.webState = status.new_active_web_state;
  }
}

#pragma mark - BookmarkModelBridgeObserver

// If an added or removed bookmark is the same as the current url, update the
// toolbar so the star highlight is kept in sync.
- (void)didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  [self updateBookmarkItem];
}

// If all bookmarks are removed, update the toolbar so the star highlight is
// kept in sync.
- (void)bookmarkModelRemovedAllNodes {
  [self updateBookmarkItem];
}

// In case we are on a bookmarked page before the model is loaded.
- (void)bookmarkModelLoaded {
  [self updateBookmarkItem];
}

- (void)didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  [self updateBookmarkItem];
}

- (void)didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
         fromParent:(const bookmarks::BookmarkNode*)oldParent
           toParent:(const bookmarks::BookmarkNode*)newParent {
  // No-op -- required by BookmarkModelBridgeObserver but not used.
}
- (void)didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  [self updateBookmarkItem];
}

#pragma mark - OverlayPresenterObserving

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

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == bookmarks::prefs::kEditBookmarksEnabled)
    [self updateBookmarkItem];
}

#pragma mark - Properties

- (void)setWebState:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());

    _iOSLanguageDetectionTabHelperObserverBridge.reset();
  }

  _webState = webState;

  if (_webState) {
    _webState->AddObserver(_webStateObserver.get());

    // Observer the language::IOSLanguageDetectionTabHelper for `_webState`.
    _iOSLanguageDetectionTabHelperObserverBridge =
        std::make_unique<language::IOSLanguageDetectionTabHelperObserverBridge>(
            language::IOSLanguageDetectionTabHelper::FromWebState(_webState),
            self);
    if (self.popupMenu) {
      [self updatePopupMenu];
    }
  }
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  _webStateList = webStateList;
  self.webState = nil;

  if (_webStateList) {
    self.webState = self.webStateList->GetActiveWebState();
    _webStateList->AddObserver(_webStateListObserver.get());
  }
}

- (void)setWebContentAreaOverlayPresenter:
    (OverlayPresenter*)webContentAreaOverlayPresenter {
  if (_webContentAreaOverlayPresenter) {
    _webContentAreaOverlayPresenter->RemoveObserver(
        _overlayPresenterObserver.get());
    self.webContentAreaShowingOverlay = NO;
  }

  _webContentAreaOverlayPresenter = webContentAreaOverlayPresenter;

  if (_webContentAreaOverlayPresenter) {
    _webContentAreaOverlayPresenter->AddObserver(
        _overlayPresenterObserver.get());
    self.webContentAreaShowingOverlay =
        _webContentAreaOverlayPresenter->IsShowingOverlayUI();
  }
}

- (void)setPopupMenu:(id<PopupMenuConsumer>)popupMenu {
  _popupMenu = popupMenu;

  [_popupMenu setPopupMenuItems:self.items];
  if (self.webState) {
    [self updatePopupMenu];
  }
}

- (void)setEngagementTracker:(feature_engagement::Tracker*)engagementTracker {
  _engagementTracker = engagementTracker;

  if (!self.popupMenu || !engagementTracker)
    return;

  if (self.readingListItem &&
      self.engagementTracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHBadgedReadingListFeature)) {
    self.readingListItem.badgeText = l10n_util::GetNSStringWithFixup(
        IDS_IOS_TOOLS_MENU_CELL_NEW_FEATURE_BADGE);
    [self.popupMenu itemsHaveChanged:@[ self.readingListItem ]];
  }

  if (self.translateItem &&
      self.engagementTracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHBadgedTranslateManualTriggerFeature)) {
    self.translateItem.badgeText = l10n_util::GetNSStringWithFixup(
        IDS_IOS_TOOLS_MENU_CELL_NEW_FEATURE_BADGE);
    [self.popupMenu itemsHaveChanged:@[ self.translateItem ]];
  }
}

- (void)setBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel {
  _bookmarkModel = bookmarkModel;
  _bookmarkModelBridge.reset();
  if (bookmarkModel) {
    _bookmarkModelBridge =
        std::make_unique<BookmarkModelBridge>(self, bookmarkModel);
  }

  if (self.webState && self.popupMenu) {
    [self updateBookmarkItem];
  }
}

- (NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>*)items {
  if (!_items) {
    [self createToolsMenuItems];
    if (self.webState && self.followItem) {
      FollowTabHelper* followTabHelper =
          FollowTabHelper::FromWebState(self.webState);
      if (followTabHelper) {
        followTabHelper->SetFollowMenuUpdater(self);
      }
    }
    NSMutableArray* specificItems = [NSMutableArray array];
    if (self.reloadStopItem)
      [specificItems addObject:self.reloadStopItem];
    if (self.readLaterItem)
      [specificItems addObject:self.readLaterItem];
    if (self.bookmarkItem)
      [specificItems addObject:self.bookmarkItem];
    if (self.translateItem)
      [specificItems addObject:self.translateItem];
    if (self.findInPageItem)
      [specificItems addObject:self.findInPageItem];
    if (self.textZoomItem)
      [specificItems addObject:self.textZoomItem];
    if (self.siteInformationItem)
      [specificItems addObject:self.siteInformationItem];
    if (self.requestDesktopSiteItem)
      [specificItems addObject:self.requestDesktopSiteItem];
    if (self.requestMobileSiteItem)
      [specificItems addObject:self.requestMobileSiteItem];
    if (self.readingListItem)
      [specificItems addObject:self.readingListItem];
    if (self.priceNotificationsItem)
      [specificItems addObject:self.priceNotificationsItem];
    self.specificItems = specificItems;
  }
  return _items;
}

- (void)setWebContentAreaShowingOverlay:(BOOL)webContentAreaShowingOverlay {
  if (_webContentAreaShowingOverlay == webContentAreaShowingOverlay)
    return;
  _webContentAreaShowingOverlay = webContentAreaShowingOverlay;
  [self updatePopupMenu];
}

- (void)setPrefService:(PrefService*)prefService {
  _prefService = prefService;
  _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
  _prefChangeRegistrar->Init(prefService);
  _prefObserverBridge.reset(new PrefObserverBridge(self));
  _prefObserverBridge->ObserveChangesForPreference(
      bookmarks::prefs::kEditBookmarksEnabled, _prefChangeRegistrar.get());
}

#pragma mark - PopupMenuActionHandlerDelegate

- (void)readPageLater {
  web::WebState* webState = self.webState;
  if (!webState)
    return;

  reading_list::AddToReadingListUsingCanonicalUrl(self.readingListBrowserAgent,
                                                  webState);
}

- (void)recordSettingsMetricsPerProfile {
  profile_metrics::BrowserProfileType type =
      _isIncognito ? profile_metrics::BrowserProfileType::kIncognito
                   : profile_metrics::BrowserProfileType::kRegular;
  base::UmaHistogramEnumeration("Settings.OpenSettingsFromMenu.PerProfileType",
                                type);
}

- (void)recordDownloadsMetricsPerProfile {
  profile_metrics::BrowserProfileType type =
      _isIncognito ? profile_metrics::BrowserProfileType::kIncognito
                   : profile_metrics::BrowserProfileType::kRegular;
  base::UmaHistogramEnumeration("Download.OpenDownloadsFromMenu.PerProfileType",
                                type);
}

- (void)searchCopiedImage {
  __weak PopupMenuMediator* weakSelf = self;
  ClipboardRecentContent* clipboardRecentContent =
      ClipboardRecentContent::GetInstance();
  clipboardRecentContent->GetRecentImageFromClipboard(
      base::BindOnce(^(std::optional<gfx::Image> optionalImage) {
        [weakSelf searchCopiedImage:optionalImage usingLens:NO];
      }));
}

- (void)lensCopiedImage {
  __weak PopupMenuMediator* weakSelf = self;
  ClipboardRecentContent* clipboardRecentContent =
      ClipboardRecentContent::GetInstance();
  clipboardRecentContent->GetRecentImageFromClipboard(
      base::BindOnce(^(std::optional<gfx::Image> optionalImage) {
        [weakSelf searchCopiedImage:optionalImage usingLens:YES];
      }));
}

- (void)toggleFollowed {
  DCHECK(IsWebChannelsEnabled());
  DCHECK(self.followBrowserAgent);

  if (self.followed) {
    self.followBrowserAgent->UnfollowWebSite(self.webPage,
                                             FollowSource::PopupMenu);
  } else {
    self.followBrowserAgent->FollowWebSite(self.webPage,
                                           FollowSource::PopupMenu);
  }
}

- (web::WebState*)currentWebState {
  return self.webState;
}

#pragma mark - IOSLanguageDetectionTabHelperObserving

- (void)iOSLanguageDetectionTabHelper:
            (language::IOSLanguageDetectionTabHelper*)tabHelper
                 didDetermineLanguage:
                     (const translate::LanguageDetectionDetails&)details {
  if (!self.translateItem)
    return;
  // Update the translate item state once language details have been determined.
  self.translateItem.enabled = [self isTranslateEnabled];
  [self.popupMenu itemsHaveChanged:@[ self.translateItem ]];
}

#pragma mark - ReadingListMenuNotificationDelegate Implementation

- (void)unreadCountChanged:(NSInteger)unreadCount {
  if (!self.readingListItem)
    return;

  self.readingListItem.badgeNumber = unreadCount;
  [self.popupMenu itemsHaveChanged:@[ self.readingListItem ]];
}

#pragma mark - FollowMenuUpdater

- (void)updateFollowMenuItemWithWebPage:(WebPageURLs*)webPage
                               followed:(BOOL)followed
                             domainName:(NSString*)domainName
                                enabled:(BOOL)enabled {
  DCHECK(IsWebChannelsEnabled());
  self.webPage = webPage;
  self.followed = followed;
  self.followItem.enabled = enabled;
  self.followItem.title =
      followed ? l10n_util::GetNSStringF(IDS_IOS_TOOLS_MENU_UNFOLLOW, u"")
               : l10n_util::GetNSStringF(IDS_IOS_TOOLS_MENU_FOLLOW, u"");
  self.followItem.image = [[UIImage
      imageNamed:followed ? @"popup_menu_unfollow" : @"popup_menu_follow"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

  [self.popupMenu itemsHaveChanged:@[ self.followItem ]];
}

#pragma mark - BrowserContainerConsumer

- (void)setContentBlocked:(BOOL)contentBlocked {
  if (_contentBlocked == contentBlocked) {
    return;
  }
  _contentBlocked = contentBlocked;
  [self updatePopupMenu];
}

#pragma mark - Popup updates (Private)

// Updates the popup menu to have its state in sync with the current page
// status.
- (void)updatePopupMenu {
  [self updateReloadStopItem];
  // The "Add to Reading List" functionality requires JavaScript execution,
  // which is paused while overlays are displayed over the web content area.
  self.readLaterItem.enabled =
      !self.webContentAreaShowingOverlay && [self isCurrentURLWebURL];
  [self updateBookmarkItem];
  self.translateItem.enabled = [self isTranslateEnabled];
  self.findInPageItem.enabled = [self isFindInPageEnabled];
  self.textZoomItem.enabled = [self isTextZoomEnabled];
  self.siteInformationItem.enabled = [self currentWebPageSupportsSiteInfo];
  self.requestDesktopSiteItem.enabled =
      [self userAgentType] == web::UserAgentType::MOBILE;
  self.requestMobileSiteItem.enabled =
      [self userAgentType] == web::UserAgentType::DESKTOP;

  // Update follow menu item.
  if (self.followItem &&
      GetFollowActionState(self.webState) != FollowActionStateHidden) {
    DCHECK(IsWebChannelsEnabled());
    FollowTabHelper* followTabHelper =
        FollowTabHelper::FromWebState(self.webState);
    if (followTabHelper) {
      followTabHelper->UpdateFollowMenuItem();
    }
  }

  // Reload the items.
  [self.popupMenu itemsHaveChanged:self.specificItems];
}

// Updates `self.bookmarkItem` to match the bookmarked status of the page.
- (void)updateBookmarkItem {
  if (!self.bookmarkItem)
    return;

  self.bookmarkItem.enabled =
      [self isCurrentURLWebURL] && [self isEditBookmarksEnabled];

  if (self.webState && self.bookmarkModel &&
      self.bookmarkModel->IsBookmarked(self.webState->GetVisibleURL())) {
    self.bookmarkItem.title =
        l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_EDIT_BOOKMARK);
    self.bookmarkItem.accessibilityIdentifier = kToolsMenuEditBookmark;
    self.bookmarkItem.image = [[UIImage imageNamed:@"popup_menu_edit_bookmark"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  } else {
    self.bookmarkItem.title =
        l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_ADD_TO_BOOKMARKS);
    self.bookmarkItem.accessibilityIdentifier = kToolsMenuAddToBookmarks;
    self.bookmarkItem.image = [[UIImage imageNamed:@"popup_menu_add_bookmark"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }

  [self.popupMenu itemsHaveChanged:@[ self.bookmarkItem ]];
}

// Updates the `reloadStopItem` item to match the current behavior.
- (void)updateReloadStopItem {
  if ([self isPageLoading] &&
      self.reloadStopItem.accessibilityIdentifier == kToolsMenuReload) {
    self.reloadStopItem.title = l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_STOP);
    self.reloadStopItem.actionIdentifier = PopupMenuActionStop;
    self.reloadStopItem.accessibilityIdentifier = kToolsMenuStop;
    self.reloadStopItem.image = [[UIImage imageNamed:@"popup_menu_stop"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  } else if (![self isPageLoading] &&
             self.reloadStopItem.accessibilityIdentifier == kToolsMenuStop) {
    self.reloadStopItem.title =
        l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_RELOAD);
    self.reloadStopItem.actionIdentifier = PopupMenuActionReload;
    self.reloadStopItem.accessibilityIdentifier = kToolsMenuReload;
    self.reloadStopItem.image = [[UIImage imageNamed:@"popup_menu_reload"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }
}

// Whether the current web page has available site info.
- (BOOL)currentWebPageSupportsSiteInfo {
  if (!self.webState)
    return NO;
  web::NavigationItem* navItem =
      self.webState->GetNavigationManager()->GetVisibleItem();
  if (!navItem) {
    return NO;
  }
  const GURL& URL = navItem->GetURL();
  // Show site info for offline pages.
  if (URL.SchemeIs(kChromeUIScheme) && URL.host() == kChromeUIOfflineHost) {
    return YES;
  }
  // Do not show site info for NTP.
  if (URL.spec() == kChromeUIAboutNewTabURL ||
      URL.spec() == kChromeUINewTabURL) {
    return NO;
  }

  if (self.contentBlocked) {
    return NO;
  }

  return navItem->GetVirtualURL().is_valid();
}

// Whether the current page is a web page.
- (BOOL)isCurrentURLWebURL {
  if (!self.webState)
    return NO;
  const GURL& URL = self.webState->GetLastCommittedURL();
  return URL.is_valid() && !web::GetWebClient()->IsAppSpecificURL(URL);
}

// Whether the translate menu item should be enabled.
- (BOOL)isTranslateEnabled {
  if (!self.webState)
    return NO;

  auto* translate_client =
      ChromeIOSTranslateClient::FromWebState(self.webState);
  if (!translate_client)
    return NO;

  translate::TranslateManager* translate_manager =
      translate_client->GetTranslateManager();
  DCHECK(translate_manager);
  return translate_manager->CanManuallyTranslate();
}

// Determines whether or not translate is available on the page and logs the
// result. This method should only be called once per popup menu shown.
- (void)logTranslateAvailability {
  if (!self.webState)
    return;

  auto* translate_client =
      ChromeIOSTranslateClient::FromWebState(self.webState);
  if (!translate_client)
    return;

  translate::TranslateManager* translate_manager =
      translate_client->GetTranslateManager();
  DCHECK(translate_manager);
  translate_manager->CanManuallyTranslate(true);
}

// Whether find in page is enabled.
- (BOOL)isFindInPageEnabled {
  if (!self.webState)
    return NO;
  auto* helper = GetConcreteFindTabHelperFromWebState(self.webState);
  return (helper && helper->CurrentPageSupportsFindInPage() &&
          !helper->IsFindUIActive());
}

// Whether or not text zoom is enabled for this page.
- (BOOL)isTextZoomEnabled {
  if (self.webContentAreaShowingOverlay) {
    return NO;
  }

  if (!self.webState) {
    return NO;
  }
  FontSizeTabHelper* helper = FontSizeTabHelper::FromWebState(self.webState);
  return helper && helper->CurrentPageSupportsTextZoom() &&
         !helper->IsTextZoomUIActive();
}

// Whether the page is currently loading.
- (BOOL)isPageLoading {
  if (!self.webState)
    return NO;
  return self.webState->IsLoading();
}

#pragma mark - Item creation (Private)

// Creates the menu items for the tools menu.
- (void)createToolsMenuItems {
  // Reload or stop page action, created as reload.
  self.reloadStopItem =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_RELOAD, PopupMenuActionReload,
                          @"popup_menu_reload", kToolsMenuReload);

  NSArray* tabActions = [@[ self.reloadStopItem ]
      arrayByAddingObjectsFromArray:[self itemsForNewTab]];

  if (base::ios::IsMultipleScenesSupported()) {
    tabActions =
        [tabActions arrayByAddingObjectsFromArray:[self itemsForNewWindow]];
  }

  NSArray* browserActions = [self actionItems];

  NSArray* collectionActions = [self collectionItems];

  if (_browserPolicyConnector &&
      _browserPolicyConnector->HasMachineLevelPolicies()) {
    // Show enterprise infomation when chrome is managed by policy and the
    // settings UI flag is enabled.
    NSArray* textActions = [self enterpriseInfoSection];
    self.items =
        @[ tabActions, collectionActions, browserActions, textActions ];
  } else {
    self.items = @[ tabActions, collectionActions, browserActions ];
  }
}

- (NSArray<TableViewItem*>*)itemsForNewTab {
  // Open New Tab.
  PopupMenuToolsItem* openNewTabItem =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_NEW_TAB, PopupMenuActionOpenNewTab,
                          @"popup_menu_new_tab", kToolsMenuNewTabId);

  // Disable the new tab menu item if the incognito mode is forced by enterprise
  // policy.
  openNewTabItem.enabled = !IsIncognitoModeForced(self.prefService);

  // Open New Incognito Tab.
  self.openNewIncognitoTabItem = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB, PopupMenuActionOpenNewIncognitoTab,
      @"popup_menu_new_incognito_tab", kToolsMenuNewIncognitoTabId);

  // Disable the new incognito tab menu item if the incognito mode is disabled
  // by enterprise policy.
  self.openNewIncognitoTabItem.enabled =
      !IsIncognitoModeDisabled(self.prefService);

  return @[ openNewTabItem, self.openNewIncognitoTabItem ];
}

- (NSArray<TableViewItem*>*)itemsForNewWindow {
  if (!base::ios::IsMultipleScenesSupported())
    return @[];

  // Create the menu item -- hardcoded string and no accessibility ID.
  PopupMenuToolsItem* openNewWindowItem = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_NEW_WINDOW, PopupMenuActionOpenNewWindow,
      @"popup_menu_new_window", kToolsMenuNewWindowId);

  return @[ openNewWindowItem ];
}

- (NSArray<TableViewItem*>*)actionItems {
  NSMutableArray* actionsArray = [NSMutableArray array];

  if (GetFollowActionState(self.webState) != FollowActionStateHidden) {
    // Follow.
    self.followItem =
        CreateFollowItem(IDS_IOS_TOOLS_MENU_FOLLOW, PopupMenuActionFollow,
                         @"popup_menu_follow", kToolsMenuFollowId);
    [actionsArray addObject:self.followItem];
  }
  // Read Later.
  self.readLaterItem = CreateTableViewItem(
      IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST, PopupMenuActionReadLater,
      @"popup_menu_read_later", kToolsMenuReadLater);
  [actionsArray addObject:self.readLaterItem];

  self.priceNotificationsItem = CreateTableViewItem(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TITLE,
      PopupMenuActionPriceNotifications, @"popup_menu_price_notifications",
      kToolsMenuPriceNotifications);
  if (self.webState && IsPriceTrackingEnabled(ProfileIOS::FromBrowserState(
                           self.webState->GetBrowserState()))) {
    [actionsArray addObject:self.priceNotificationsItem];
  }

  // Add to bookmark.
  self.bookmarkItem = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_ADD_TO_BOOKMARKS, PopupMenuActionPageBookmark,
      @"popup_menu_add_bookmark", kToolsMenuAddToBookmarks);
  [actionsArray addObject:self.bookmarkItem];

  // Translate.
  [self logTranslateAvailability];
  self.translateItem = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_TRANSLATE, PopupMenuActionTranslate,
      @"popup_menu_translate", kToolsMenuTranslateId);
  if (self.engagementTracker &&
      self.engagementTracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHBadgedTranslateManualTriggerFeature) &&
      self.isTranslateEnabled) {
    self.translateItem.badgeText = l10n_util::GetNSStringWithFixup(
        IDS_IOS_TOOLS_MENU_CELL_NEW_FEATURE_BADGE);
  }
  [actionsArray addObject:self.translateItem];

  // Find in Page.
  self.findInPageItem = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_FIND_IN_PAGE, PopupMenuActionFindInPage,
      @"popup_menu_find_in_page", kToolsMenuFindInPageId);
  [actionsArray addObject:self.findInPageItem];

  // Text Zoom
  if (ios::provider::IsTextZoomEnabled()) {
    self.textZoomItem = CreateTableViewItem(
        IDS_IOS_TOOLS_MENU_TEXT_ZOOM, PopupMenuActionTextZoom,
        @"popup_menu_text_zoom", kToolsMenuTextZoom);
    [actionsArray addObject:self.textZoomItem];
  }

  if ([self userAgentType] != web::UserAgentType::DESKTOP) {
    // Request Desktop Site.
    self.requestDesktopSiteItem = CreateTableViewItem(
        IDS_IOS_TOOLS_MENU_REQUEST_DESKTOP_SITE, PopupMenuActionRequestDesktop,
        @"popup_menu_request_desktop_site", kToolsMenuRequestDesktopId);
    // Disable the action if the user agent is not mobile.
    self.requestDesktopSiteItem.enabled =
        [self userAgentType] == web::UserAgentType::MOBILE;
    [actionsArray addObject:self.requestDesktopSiteItem];
  } else {
    // Request Mobile Site.
    self.requestMobileSiteItem = CreateTableViewItem(
        IDS_IOS_TOOLS_MENU_REQUEST_MOBILE_SITE, PopupMenuActionRequestMobile,
        @"popup_menu_request_mobile_site", kToolsMenuRequestMobileId);
    [actionsArray addObject:self.requestMobileSiteItem];
  }

  // Site Information.
  self.siteInformationItem = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_SITE_INFORMATION, PopupMenuActionSiteInformation,
      @"popup_menu_site_information", kToolsMenuSiteInformation);
  [actionsArray addObject:self.siteInformationItem];

  // Report an Issue.
  if (ios::provider::IsUserFeedbackSupported()) {
    TableViewItem* reportIssue = CreateTableViewItem(
        IDS_IOS_OPTIONS_REPORT_AN_ISSUE, PopupMenuActionReportIssue,
        @"popup_menu_report_an_issue", kToolsMenuReportAnIssueId);
    [actionsArray addObject:reportIssue];
  }

  // Help.
  TableViewItem* help =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_HELP_MOBILE, PopupMenuActionHelp,
                          @"popup_menu_help", kToolsMenuHelpId);
  [actionsArray addObject:help];

#if !defined(NDEBUG)
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  if ([standardDefaults boolForKey:@"DevViewSource"]) {
    PopupMenuToolsItem* item =
        [[PopupMenuToolsItem alloc] initWithType:kItemTypeEnumZero];
    item.title = @"View Source";
    item.actionIdentifier = PopupMenuActionViewSource;
    item.accessibilityIdentifier = @"View Source";

    // Debug menu, not localized, only visible if turned on by a default.
    [actionsArray addObject:item];
  }
#endif  // !defined(NDEBUG)

  return actionsArray;
}

- (NSArray<TableViewItem*>*)collectionItems {
  // Bookmarks.
  TableViewItem* bookmarks = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_BOOKMARKS, PopupMenuActionBookmarks,
      @"popup_menu_bookmarks", kToolsMenuBookmarksId);

  // Reading List.
  if (self.readingListModelLoaded) {
    self.readingListItem = CreateTableViewItem(
        IDS_IOS_TOOLS_MENU_READING_LIST, PopupMenuActionReadingList,
        @"popup_menu_reading_list", kToolsMenuReadingListId);
    NSInteger numberOfUnreadArticles =
        [self.readingListMenuNotifier readingListUnreadCount];
    self.readingListItem.badgeNumber = numberOfUnreadArticles;
    if (numberOfUnreadArticles) {
      self.readingListItem.additionalAccessibilityLabel =
          AccessibilityLabelForReadingListCellWithCount(numberOfUnreadArticles);
    }
    if (self.engagementTracker &&
        self.engagementTracker->ShouldTriggerHelpUI(
            feature_engagement::kIPHBadgedReadingListFeature)) {
      self.readingListItem.badgeText = l10n_util::GetNSStringWithFixup(
          IDS_IOS_TOOLS_MENU_CELL_NEW_FEATURE_BADGE);
    }
  }

  // Recent Tabs.
  PopupMenuToolsItem* recentTabs = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_RECENT_TABS, PopupMenuActionRecentTabs,
      @"popup_menu_recent_tabs", kToolsMenuOtherDevicesId);
  recentTabs.enabled = !self.isIncognito;

  // History.
  PopupMenuToolsItem* history =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_HISTORY, PopupMenuActionHistory,
                          @"popup_menu_history", kToolsMenuHistoryId);
  history.enabled = !self.isIncognito;

  // Open Downloads folder.
  TableViewItem* downloadsFolder = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_DOWNLOADS, PopupMenuActionOpenDownloads,
      @"popup_menu_downloads", kToolsMenuDownloadsId);

  // Settings.
  TableViewItem* settings =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_SETTINGS, PopupMenuActionSettings,
                          @"popup_menu_settings", kToolsMenuSettingsActionId);

  NSMutableArray* items = [[NSMutableArray alloc] init];
  [items addObject:bookmarks];
  if (self.readingListItem) {
    [items addObject:self.readingListItem];
  }
  if (!self.isIncognito) {
    [items addObject:recentTabs];
    [items addObject:history];
  }
  [items addObject:downloadsFolder];
  [items addObject:settings];
  return items;
}

// Creates the section for enterprise info.
- (NSArray<TableViewItem*>*)enterpriseInfoSection {
  NSString* message = l10n_util::GetNSString(IDS_IOS_ENTERPRISE_MANAGED_INFO);
  TableViewItem* enterpriseInfoItem = CreateEnterpriseInfoItem(
      @"popup_menu_enterprise_icon", message,
      PopupMenuActionEnterpriseInfoMessage, kTextMenuEnterpriseInfo);
  return @[ enterpriseInfoItem ];
}

// Returns the UserAgentType currently in use.
- (web::UserAgentType)userAgentType {
  if (!self.webState)
    return web::UserAgentType::NONE;
  web::NavigationItem* visibleItem =
      self.webState->GetNavigationManager()->GetVisibleItem();
  if (!visibleItem)
    return web::UserAgentType::NONE;

  return visibleItem->GetUserAgentType();
}

#pragma mark - Other private methods

// Returns YES if user is allowed to edit any bookmarks.
- (BOOL)isEditBookmarksEnabled {
    return self.prefService->GetBoolean(
        bookmarks::prefs::kEditBookmarksEnabled);
}

// Returns YES if incognito NTP title and image should be used for back/forward
// item associated with `URL`.
- (BOOL)shouldUseIncognitoNTPResourcesForURL:(const GURL&)URL {
    return URL.DeprecatedGetOriginAsURL() == kChromeUINewTabURL &&
           self.isIncognito;
}

// Searches the copied image. If `usingLens` is set, then the search will be
// performed with Lens.
- (void)searchCopiedImage:(std::optional<gfx::Image>)optionalImage
                usingLens:(BOOL)usingLens {
  if (!optionalImage)
    return;

  UIImage* image = optionalImage->ToUIImage();
  if (usingLens) {
    SearchImageWithLensCommand* command = [[SearchImageWithLensCommand alloc]
        initWithImage:image
           entryPoint:LensEntrypoint::OmniboxPostCapture];
    [self.lensCommandsHandler searchImageWithLens:command];
  } else {
    web::NavigationManager::WebLoadParams webParams =
        ImageSearchParamGenerator::LoadParamsForImage(image,
                                                      self.templateURLService);
    UrlLoadParams params = UrlLoadParams::InCurrentTab(webParams);

    self.URLLoadingBrowserAgent->Load(params);
  }
}

@end
