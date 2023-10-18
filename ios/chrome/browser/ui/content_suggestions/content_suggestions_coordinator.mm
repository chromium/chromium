// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"

#import <vector>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/public/features.h"
#import "components/sync/base/features.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_cache_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/favicon/large_icon_cache.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/set_up_list_prefs.h"
#import "ios/chrome/browser/ntp_tiles/ios_most_visited_sites_factory.h"
#import "ios/chrome/browser/passwords/password_checkup_utils.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/promos_manager/promos_manager_factory.h"
#import "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager_factory.h"
#import "ios/chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_menu_provider.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_table_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack_parcel_list_half_sheet_table_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_view.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/types.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/utils.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_default_browser_promo_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_default_browser_promo_coordinator_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_show_more_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_view.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/ntp/feed_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_metrics_delegate.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

@interface ContentSuggestionsCoordinator () <
    ContentSuggestionsMenuProvider,
    ContentSuggestionsViewControllerAudience,
    MagicStackHalfSheetTableViewControllerDelegate,
    SafetyCheckViewDelegate,
    SetUpListDefaultBrowserPromoCoordinatorDelegate,
    MagicStackParcelListHalfSheetTableViewControllerDelegate,
    SetUpListViewDelegate>

@property(nonatomic, strong)
    ContentSuggestionsViewController* contentSuggestionsViewController;
@property(nonatomic, assign) BOOL contentSuggestionsEnabled;
// Authentication Service for the user's signed-in state.
@property(nonatomic, assign) AuthenticationService* authService;
// Coordinator in charge of handling sharing use cases.
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;
// Redefined to not be readonly.
@property(nonatomic, strong)
    ContentSuggestionsMediator* contentSuggestionsMediator;
// Metrics recorder for the content suggestions.
@property(nonatomic, strong)
    ContentSuggestionsMetricsRecorder* contentSuggestionsMetricsRecorder;

@end

@implementation ContentSuggestionsCoordinator {
  // Observer bridge for mediator to listen to
  // StartSurfaceRecentTabObserverBridge.
  std::unique_ptr<StartSurfaceRecentTabObserverBridge> _startSurfaceObserver;

  // The coordinator that displays the Default Browser Promo for the Set Up
  // List.
  SetUpListDefaultBrowserPromoCoordinator* _defaultBrowserPromoCoordinator;

  // The coordinator used to present an action sheet for the Set Up List menu.
  ActionSheetCoordinator* _actionSheetCoordinator;

  // The Show More Menu presented from the Set Up List in the Magic Stack.
  SetUpListShowMoreViewController* _setUpListShowMoreViewController;

  // The edit half sheet for toggling all Magic Stack modules.
  MagicStackHalfSheetTableViewController*
      _magicStackHalfSheetTableViewController;

  // The parcel list half sheet to see all tracked parcels.
  MagicStackParcelListHalfSheetTableViewController*
      _parcelListHalfSheetTableViewController;

  // The coordinator used to present a modal alert for the parcel tracking
  // module.
  AlertCoordinator* _parcelTrackingAlertCoordinator;
}

- (void)start {
  DCHECK(self.browser);
  DCHECK(self.NTPMetricsDelegate);

  if (self.started) {
    // Prevent this coordinator from being started twice in a row
    return;
  }

  _started = YES;

  self.authService = AuthenticationServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());

  PrefService* prefs =
      ChromeBrowserState::FromBrowserState(self.browser->GetBrowserState())
          ->GetPrefs();

  self.contentSuggestionsEnabled =
      prefs->GetBoolean(prefs::kArticlesForYouEnabled) &&
      prefs->GetBoolean(prefs::kNTPContentSuggestionsEnabled);

  favicon::LargeIconService* largeIconService =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  LargeIconCache* cache = IOSChromeLargeIconCacheFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  std::unique_ptr<ntp_tiles::MostVisitedSites> mostVisitedFactory =
      IOSMostVisitedSitesFactory::NewForBrowserState(
          self.browser->GetBrowserState());
  ReadingListModel* readingListModel =
      ReadingListModelFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  PromosManager* promosManager =
      PromosManagerFactory::GetForBrowserState(self.browser->GetBrowserState());

  BOOL isGoogleDefaultSearchProvider =
      [self.NTPDelegate isGoogleDefaultSearchEngine];

  self.contentSuggestionsMetricsRecorder =
      [[ContentSuggestionsMetricsRecorder alloc]
          initWithLocalState:GetApplicationContext()->GetLocalState()];

  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState());

  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  commerce::ShoppingService* shoppingService =
      commerce::ShoppingServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  self.contentSuggestionsMediator = [[ContentSuggestionsMediator alloc]
           initWithLargeIconService:largeIconService
                     largeIconCache:cache
                    mostVisitedSite:std::move(mostVisitedFactory)
                   readingListModel:readingListModel
                        prefService:prefs
      isGoogleDefaultSearchProvider:isGoogleDefaultSearchProvider
                        syncService:syncService
              authenticationService:authenticationService
                    identityManager:identityManager
                    shoppingService:shoppingService
                            browser:self.browser];
  self.contentSuggestionsMediator.feedDelegate = self.feedDelegate;
  self.contentSuggestionsMediator.promosManager = promosManager;
  self.contentSuggestionsMediator.contentSuggestionsMetricsRecorder =
      self.contentSuggestionsMetricsRecorder;
  if (base::FeatureList::IsEnabled(segmentation_platform::features::
                                       kSegmentationPlatformIosModuleRanker)) {
    self.contentSuggestionsMediator.segmentationService =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetForBrowserState(self.browser->GetBrowserState());
  }
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  self.contentSuggestionsMediator.dispatcher =
      static_cast<id<ApplicationCommands, BrowserCoordinatorCommands,
                     OmniboxCommands, SnackbarCommands>>(
          self.browser->GetCommandDispatcher());
  self.contentSuggestionsMediator.webStateList =
      self.browser->GetWebStateList();
  self.contentSuggestionsMediator.webState = self.webState;
  self.contentSuggestionsMediator.NTPMetricsDelegate = self.NTPMetricsDelegate;

  self.contentSuggestionsViewController =
      [[ContentSuggestionsViewController alloc] init];
  self.contentSuggestionsViewController.suggestionCommandHandler =
      self.contentSuggestionsMediator;
  self.contentSuggestionsViewController.audience = self;
  self.contentSuggestionsViewController.menuProvider = self;
  self.contentSuggestionsViewController.urlLoadingBrowserAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  self.contentSuggestionsViewController.contentSuggestionsMetricsRecorder =
      self.contentSuggestionsMetricsRecorder;
  self.contentSuggestionsViewController.setUpListViewDelegate = self;

  self.contentSuggestionsMediator.consumer =
      self.contentSuggestionsViewController;
}

- (void)stop {
  // Reset the observer bridge object before setting
  // `contentSuggestionsMediator` nil.
  if (_startSurfaceObserver) {
    StartSurfaceRecentTabBrowserAgent::FromBrowser(self.browser)
        ->RemoveObserver(_startSurfaceObserver.get());
    _startSurfaceObserver.reset();
  }
  [self.contentSuggestionsMediator disconnect];
  self.contentSuggestionsMediator = nil;
  [self.contentSuggestionsMetricsRecorder disconnect];
  self.contentSuggestionsMetricsRecorder = nil;
  self.contentSuggestionsViewController.audience = nil;
  self.contentSuggestionsViewController = nil;
  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;
  [_defaultBrowserPromoCoordinator stop];
  _defaultBrowserPromoCoordinator = nil;
  [_magicStackHalfSheetTableViewController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:nil];
  _magicStackHalfSheetTableViewController = nil;
  [self dismissParcelListHalfSheet];
  [self dismissParcelTrackingAlertCoordinator];
  _started = NO;
}

- (UIViewController*)viewController {
  return self.contentSuggestionsViewController;
}

#pragma mark - Setters

- (void)setWebState:(web::WebState*)webState {
  _webState = webState;
  self.contentSuggestionsMediator.webState = webState;
}

#pragma mark - ContentSuggestionsViewControllerAudience

- (void)viewWillDisappear {
  DiscoverFeedServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState())
      ->SetIsShownOnStartSurface(false);
  [self.contentSuggestionsMediator hideRecentTabTile];
}

- (void)returnToRecentTabWasAdded {
  [self.NTPDelegate updateFeedLayout];
  [self.NTPDelegate setContentOffsetToTop];
}

- (void)moduleWasRemoved {
  [self.NTPDelegate updateFeedLayout];
}

- (UIEdgeInsets)safeAreaInsetsForDiscoverFeed {
  return [SceneStateBrowserAgent::FromBrowser(self.browser)
              ->GetSceneState()
              .window.rootViewController.view safeAreaInsets];
}

- (void)neverShowModuleType:(ContentSuggestionsModuleType)type {
  switch (type) {
    case ContentSuggestionsModuleType::kTabResumption:
      [self.contentSuggestionsMediator disableTabResumption];
      break;
    case ContentSuggestionsModuleType::kSafetyCheck:
    case ContentSuggestionsModuleType::kSafetyCheckMultiRow:
    case ContentSuggestionsModuleType::kSafetyCheckMultiRowOverflow:
      [self.contentSuggestionsMediator disableSafetyCheck:type];
      break;
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
      [self.contentSuggestionsMediator disableSetUpList];
      break;
    case ContentSuggestionsModuleType::kParcelTracking:
    case ContentSuggestionsModuleType::kParcelTrackingSeeMore: {
      [self presentParcelTrackingAlertCoordinator];
      break;
    }
    default:
      break;
  }
}

- (void)didTapMagicStackEditButton {
  _magicStackHalfSheetTableViewController =
      [[MagicStackHalfSheetTableViewController alloc]
          initWithPrefService:GetApplicationContext()->GetLocalState()];
  _magicStackHalfSheetTableViewController.delegate = self;

  UINavigationController* navViewController = [[UINavigationController alloc]
      initWithRootViewController:_magicStackHalfSheetTableViewController];

  navViewController.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      navViewController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  presentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  [self.viewController presentViewController:navViewController
                                    animated:YES
                                  completion:nil];
}

- (void)showMagicStackParcelList {
  _parcelListHalfSheetTableViewController =
      [[MagicStackParcelListHalfSheetTableViewController alloc]
          initWithParcels:[self.contentSuggestionsMediator
                                  parcelTrackingItems]];
  _parcelListHalfSheetTableViewController.delegate = self;

  UINavigationController* navViewController = [[UINavigationController alloc]
      initWithRootViewController:_parcelListHalfSheetTableViewController];

  navViewController.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      navViewController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  presentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  [self.viewController presentViewController:navViewController
                                    animated:YES
                                  completion:nil];
}

#pragma mark - MagicStackHalfSheetTableViewControllerDelegate

- (void)dismissMagicStackHalfSheet {
  [_magicStackHalfSheetTableViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _magicStackHalfSheetTableViewController = nil;
}

#pragma mark - MagicStackParcelListHalfSheetTableViewControllerDelegate

- (void)dismissParcelListHalfSheet {
  [_parcelListHalfSheetTableViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _parcelListHalfSheetTableViewController = nil;
}

- (void)untrackParcel:(NSString*)parcelID carrier:(ParcelType)carrier {
  [self.contentSuggestionsMediator untrackParcel:parcelID];

  id<SnackbarCommands> snackbarHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  __weak __typeof(self) weakSelf = self;
  [snackbarHandler
      showSnackbarWithMessage:
          l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_UNTRACK_SNACKBAR_TITLE)
                   buttonText:l10n_util::GetNSString(
                                  IDS_IOS_SNACKBAR_ACTION_UNDO)
                messageAction:^{
                  __strong __typeof(weakSelf) strongSelf = weakSelf;
                  if (!strongSelf) {
                    return;
                  }
                  [strongSelf.contentSuggestionsMediator trackParcel:parcelID
                                                             carrier:carrier];
                }
             completionAction:nil];
}

#pragma mark - Public methods

- (UIView*)view {
  return self.contentSuggestionsViewController.view;
}

#pragma mark - ContentSuggestionsMenuProvider

- (UIContextMenuConfiguration*)contextMenuConfigurationForItem:
                                   (ContentSuggestionsMostVisitedItem*)item
                                                      fromView:(UIView*)view {
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        if (!weakSelf) {
          // Return an empty menu.
          return [UIMenu menuWithTitle:@"" children:@[]];
        }

        ContentSuggestionsCoordinator* strongSelf = weakSelf;

        // Record that this context menu was shown to the user.
        RecordMenuShown(MenuScenarioHistogram::kMostVisitedEntry);

        BrowserActionFactory* actionFactory = [[BrowserActionFactory alloc]
            initWithBrowser:strongSelf.browser
                   scenario:MenuScenarioHistogram::kMostVisitedEntry];

        NSMutableArray<UIMenuElement*>* menuElements =
            [[NSMutableArray alloc] init];

        CGPoint centerPoint = [view.superview convertPoint:view.center
                                                    toView:nil];

        [menuElements addObject:[actionFactory actionToOpenInNewTabWithBlock:^{
                        [weakSelf.contentSuggestionsMediator
                            openNewTabWithMostVisitedItem:item
                                                incognito:NO
                                                  atIndex:item.index
                                                fromPoint:centerPoint];
                      }]];

        UIAction* incognitoAction =
            [actionFactory actionToOpenInNewIncognitoTabWithBlock:^{
              [weakSelf.contentSuggestionsMediator
                  openNewTabWithMostVisitedItem:item
                                      incognito:YES
                                        atIndex:item.index
                                      fromPoint:centerPoint];
            }];

        if (IsIncognitoModeDisabled(
                self.browser->GetBrowserState()->GetPrefs())) {
          // Disable the "Open in Incognito" option if the incognito mode is
          // disabled.
          incognitoAction.attributes = UIMenuElementAttributesDisabled;
        }

        [menuElements addObject:incognitoAction];

        if (base::ios::IsMultipleScenesSupported()) {
          UIAction* newWindowAction = [actionFactory
              actionToOpenInNewWindowWithURL:item.URL
                              activityOrigin:
                                  WindowActivityContentSuggestionsOrigin];
          [menuElements addObject:newWindowAction];
        }

        [menuElements addObject:[actionFactory actionToCopyURL:item.URL]];

        [menuElements addObject:[actionFactory actionToShareWithBlock:^{
                        [weakSelf shareURL:item.URL
                                     title:item.title
                                  fromView:view];
                      }]];

        [menuElements addObject:[actionFactory actionToRemoveWithBlock:^{
                        [weakSelf.contentSuggestionsMediator
                            removeMostVisited:item];
                      }]];

        return [UIMenu menuWithTitle:@"" children:menuElements];
      };
  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

#pragma mark - SafetyCheckViewDelegate

// Called when a Safety Check item is selected by the user. Depending on the
// Safety Check item `type`, this method fires a UI command to present the
// Update Chrome page, Password Checkup, or Safety Check half sheet.
- (void)didSelectSafetyCheckItem:(SafetyCheckItemType)type {
  CHECK(IsSafetyCheckMagicStackEnabled());

  [self.NTPMetricsDelegate safetyCheckOpened];
  [self.contentSuggestionsMetricsRecorder
      recordMagicStackModuleEngagementForType:ContentSuggestionsModuleType::
                                                  kSafetyCheck];

  IOSChromeSafetyCheckManager* safetyCheckManager =
      IOSChromeSafetyCheckManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  switch (type) {
    case SafetyCheckItemType::kUpdateChrome: {
      const GURL& chrome_upgrade_url =
          safetyCheckManager->GetChromeAppUpgradeUrl();

      HandleSafetyCheckUpdateChromeTap(
          chrome_upgrade_url,
          HandlerForProtocol(self.browser->GetCommandDispatcher(),
                             ApplicationCommands));

      break;
    }
    case SafetyCheckItemType::kPassword: {
      std::vector<password_manager::CredentialUIEntry> credentials =
          safetyCheckManager->GetInsecureCredentials();

      HandleSafetyCheckPasswordTap(
          credentials, HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                          ApplicationCommands));

      break;
    }
    case SafetyCheckItemType::kSafeBrowsing:
      [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                          ApplicationSettingsCommands)
          showSafeBrowsingSettings];

      break;
    case SafetyCheckItemType::kAllSafe:
    case SafetyCheckItemType::kRunning:
    case SafetyCheckItemType::kDefault:
      [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                          ApplicationCommands)
          showAndStartSafetyCheckInHalfSheet:YES
                                    referrer:password_manager::
                                                 PasswordCheckReferrer::
                                                     kSafetyCheckMagicStack];

      break;
  }
}

#pragma mark - SetUpListViewDelegate

- (void)didSelectSetUpListItem:(SetUpListItemType)type {
  [self.contentSuggestionsMetricsRecorder recordSetUpListItemSelected:type];
  [self.NTPMetricsDelegate setUpListItemOpened];
  PrefService* localState = GetApplicationContext()->GetLocalState();
  set_up_list_prefs::RecordInteraction(localState);

  __weak ContentSuggestionsCoordinator* weakSelf = self;
  ProceduralBlock completionBlock = ^{
    switch (type) {
      case SetUpListItemType::kSignInSync:
        [weakSelf showSignIn];
        break;
      case SetUpListItemType::kDefaultBrowser:
        [weakSelf showDefaultBrowserPromo];
        break;
      case SetUpListItemType::kAutofill:
        [weakSelf showCredentialProviderPromo];
        break;
      case SetUpListItemType::kFollow:
      case SetUpListItemType::kAllSet:
        // TODO(crbug.com/1428070): Add a Follow item to the Set Up List.
        NOTREACHED();
    }
  };

  if (_setUpListShowMoreViewController) {
    [_setUpListShowMoreViewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:completionBlock];
    _setUpListShowMoreViewController = nil;
  } else {
    completionBlock();
  }
}

- (void)showSetUpListMenuWithButton:(UIButton*)button {
  _actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                           title:nil
                         message:nil
                            rect:button.bounds
                            view:button];

  __weak ContentSuggestionsMediator* weakMediator =
      self.contentSuggestionsMediator;
  [_actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_SET_UP_LIST_SETTINGS_TURN_OFF)
                action:^{
                  [weakMediator disableSetUpList];
                }
                 style:UIAlertActionStyleDestructive];
  [_actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_SET_UP_LIST_SETTINGS_CANCEL)
                action:nil
                 style:UIAlertActionStyleCancel];
  [_actionSheetCoordinator start];
}

- (void)setUpListViewHeightDidChange {
  [self.feedDelegate contentSuggestionsWasUpdated];
}

- (void)dismissSeeMoreViewController {
  DCHECK(_setUpListShowMoreViewController);
  [_setUpListShowMoreViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _setUpListShowMoreViewController = nil;
}

#pragma mark - SetUpList Helpers

// Shows the Default Browser Promo.
- (void)showDefaultBrowserPromo {
  // Stop the coordinator if it is already running. If the user swipes to
  // dismiss a previous instance and then clicks the item again the
  // previous instance may not have been stopped yet due to the animation.
  [_defaultBrowserPromoCoordinator stop];
  _defaultBrowserPromoCoordinator =
      [[SetUpListDefaultBrowserPromoCoordinator alloc]
          initWithBaseViewController:[self viewController]
                             browser:self.browser
                         application:[UIApplication sharedApplication]];
  _defaultBrowserPromoCoordinator.delegate = self;
  [_defaultBrowserPromoCoordinator start];
}

// Shows the SigninSync UI with the SetUpList access point.
- (void)showSignIn {
  ShowSigninCommandCompletionCallback callback =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* completionInfo) {
        if (result == SigninCoordinatorResultSuccess ||
            result == SigninCoordinatorResultCanceledByUser) {
          PrefService* localState = GetApplicationContext()->GetLocalState();
          set_up_list_prefs::MarkItemComplete(localState,
                                              SetUpListItemType::kSignInSync);
        }
      };
  AuthenticationOperation operation =
      AuthenticationOperation::kSigninAndSyncWithTwoScreens;
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    // If there are 0 identities, kInstantSignin requires less taps.
    ChromeBrowserState* browserState = self.browser->GetBrowserState();
    operation =
        ChromeAccountManagerServiceFactory::GetForBrowserState(browserState)
                ->HasIdentities()
            ? AuthenticationOperation::kSigninOnly
            : AuthenticationOperation::kInstantSignin;
  }
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:operation
               identity:nil
            accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_SET_UP_LIST
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:callback];
  [HandlerForProtocol(self.browser->GetCommandDispatcher(), ApplicationCommands)
              showSignin:command
      baseViewController:self.viewController];
}

// Shows the Credential Provider Promo using the SetUpList trigger.
- (void)showCredentialProviderPromo {
  [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                      CredentialProviderPromoCommands)
      showCredentialProviderPromoWithTrigger:CredentialProviderPromoTrigger::
                                                 SetUpList];
}

- (void)showSetUpListShowMoreMenu {
  NSArray<SetUpListItemViewData*>* items =
      [self.contentSuggestionsMediator allSetUpListItems];
  _setUpListShowMoreViewController =
      [[SetUpListShowMoreViewController alloc] initWithItems:items
                                                 tapDelegate:self];
  _setUpListShowMoreViewController.modalPresentationStyle =
      UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      _setUpListShowMoreViewController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  presentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  presentationController.preferredCornerRadius = 16;
  [self.viewController presentViewController:_setUpListShowMoreViewController
                                    animated:YES
                                  completion:nil];
}

#pragma mark - SetUpListDefaultBrowserPromoCoordinatorDelegate

- (void)setUpListDefaultBrowserPromoDidFinish:(BOOL)success {
  [_defaultBrowserPromoCoordinator stop];
  _defaultBrowserPromoCoordinator = nil;
}

#pragma mark - Helpers

- (void)configureStartSurfaceIfNeeded {
  SceneState* scene =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  if (!NewTabPageTabHelper::FromWebState(self.webState)
           ->ShouldShowStartSurface()) {
    return;
  }

  web::WebState* most_recent_tab =
      StartSurfaceRecentTabBrowserAgent::FromBrowser(self.browser)
          ->most_recent_tab();
  CHECK(most_recent_tab);
  [self.contentSuggestionsMetricsRecorder recordReturnToRecentTabTileShown];
  DiscoverFeedServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState())
      ->SetIsShownOnStartSurface(true);
  NSString* time_label = GetRecentTabTileTimeLabelForSceneState(scene);
  [self.contentSuggestionsMediator
      configureMostRecentTabItemWithWebState:most_recent_tab
                                   timeLabel:time_label];
  if (!_startSurfaceObserver) {
    _startSurfaceObserver =
        std::make_unique<StartSurfaceRecentTabObserverBridge>(
            self.contentSuggestionsMediator);
    StartSurfaceRecentTabBrowserAgent::FromBrowser(self.browser)
        ->AddObserver(_startSurfaceObserver.get());
  }
}

// Triggers the URL sharing flow for the given `URL` and `title`, with the
// origin `view` representing the UI component for that URL.
- (void)shareURL:(const GURL&)URL
           title:(NSString*)title
        fromView:(UIView*)view {
  SharingParams* params =
      [[SharingParams alloc] initWithURL:URL
                                   title:title
                                scenario:SharingScenario::MostVisitedEntry];
  self.sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.contentSuggestionsViewController
                         browser:self.browser
                          params:params
                      originView:view];
  [self.sharingCoordinator start];
}

// Presents the parcel tracking alert modal.
- (void)presentParcelTrackingAlertCoordinator {
  _parcelTrackingAlertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                           title:
                               l10n_util::GetNSString(
                                   IDS_IOS_PARCEL_TRACKING_MODULE_HIDE_ALERT_TITLE)
                         message:
                             l10n_util::GetNSStringF(
                                 IDS_IOS_PARCEL_TRACKING_MODULE_HIDE_ALERT_DESCRIPTION,
                                 base::SysNSStringToUTF16(l10n_util::GetNSString(
                                     IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_TITLE)))];

  __weak ContentSuggestionsCoordinator* weakSelf = self;
  __weak ContentSuggestionsMediator* weakMediator =
      self.contentSuggestionsMediator;
  [_parcelTrackingAlertCoordinator
      addItemWithTitle:
          l10n_util::GetNSStringF(
              IDS_IOS_PARCEL_TRACKING_CONTEXT_MENU_DESCRIPTION,
              base::SysNSStringToUTF16(l10n_util::GetNSString(
                  IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_TITLE)))
                action:^{
                  [weakMediator disableParcelTracking];
                  [weakSelf dismissParcelTrackingAlertCoordinator];
                }
                 style:UIAlertActionStyleDefault];
  [_parcelTrackingAlertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_PARCEL_TRACKING_MODULE_HIDE_ALERT_CANCEL)
                action:^{
                  [weakSelf dismissParcelTrackingAlertCoordinator];
                }
                 style:UIAlertActionStyleCancel];

  [_parcelTrackingAlertCoordinator start];
}

// Dismisses the parcel tracking alert modal.
- (void)dismissParcelTrackingAlertCoordinator {
  [_parcelTrackingAlertCoordinator stop];
  _parcelTrackingAlertCoordinator = nil;
}

@end
