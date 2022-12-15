// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/browser_coordinator.h"

#import <StoreKit/StoreKit.h>
#import <memory>

#import "base/metrics/histogram_functions.h"
#import "base/scoped_observation.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/signin/ios/browser/active_state_manager.h"
#import "components/translate/core/browser/translate_manager.h"
#import "ios/chrome/browser/app_launcher/app_launcher_abuse_detector.h"
#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper.h"
#import "ios/chrome/browser/autofill/autofill_tab_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/commerce/price_notifications/price_notifications_tab_helper.h"
#import "ios/chrome/browser/commerce/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/download/download_directory_util.h"
#import "ios/chrome/browser/download/external_app_util.h"
#import "ios/chrome/browser/download/pass_kit_tab_helper.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/feature_engagement/tracker_util.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#import "ios/chrome/browser/follow/follow_browser_agent.h"
#import "ios/chrome/browser/follow/follow_tab_helper.h"
#import "ios/chrome/browser/follow/followed_web_site.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/overscroll_actions/overscroll_actions_tab_helper.h"
#import "ios/chrome/browser/prerender/preload_controller_delegate.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/signin/account_consistency_browser_agent.h"
#import "ios/chrome/browser/signin/account_consistency_service_factory.h"
#import "ios/chrome/browser/ssl/captive_portal_tab_helper.h"
#import "ios/chrome/browser/store_kit/store_kit_coordinator.h"
#import "ios/chrome/browser/sync/sync_error_browser_agent.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/activity_services/activity_params.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/alert_coordinator/repost_form_coordinator.h"
#import "ios/chrome/browser/ui/app_store_rating/features.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_prompt/enterprise_prompt_coordinator.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_prompt/enterprise_prompt_type.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_coordinator.h"
#import "ios/chrome/browser/ui/badges/badge_popup_menu_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_interaction_controller.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_coordinator.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_coordinator+private.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller+delegates.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller+private.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/key_commands_provider.h"
#import "ios/chrome/browser/ui/browser_view/tab_events_mediator.h"
#import "ios/chrome/browser/ui/browser_view/tab_lifecycle_mediator.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/feed_commands.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#import "ios/chrome/browser/ui/commands/new_tab_page_commands.h"
#import "ios/chrome/browser/ui/commands/page_info_commands.h"
#import "ios/chrome/browser/ui/commands/password_breach_commands.h"
#import "ios/chrome/browser/ui/commands/password_protection_commands.h"
#import "ios/chrome/browser/ui/commands/password_suggestion_commands.h"
#import "ios/chrome/browser/ui/commands/policy_change_commands.h"
#import "ios/chrome/browser/ui/commands/price_notifications_commands.h"
#import "ios/chrome/browser/ui/commands/promos_manager_commands.h"
#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#import "ios/chrome/browser/ui/commands/share_highlight_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/commands/text_zoom_commands.h"
#import "ios/chrome/browser/ui/commands/web_content_commands.h"
#import "ios/chrome/browser/ui/commands/whats_new_commands.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_configuration_provider.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_coordinator.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_coordinator.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_scheduler.h"
#import "ios/chrome/browser/ui/default_promo/default_promo_non_modal_presentation_delegate.h"
#import "ios/chrome/browser/ui/default_promo/tailored_promo_coordinator.h"
#import "ios/chrome/browser/ui/download/ar_quick_look_coordinator.h"
#import "ios/chrome/browser/ui/download/download_manager_coordinator.h"
#import "ios/chrome/browser/ui/download/features.h"
#import "ios/chrome/browser/ui/download/pass_kit_coordinator.h"
#import "ios/chrome/browser/ui/download/safari_download_coordinator.h"
#import "ios/chrome/browser/ui/download/vcard_coordinator.h"
#import "ios/chrome/browser/ui/elements/activity_overlay_coordinator.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_controller_ios.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_coordinator.h"
#import "ios/chrome/browser/ui/follow/first_follow_coordinator.h"
#import "ios/chrome/browser/ui/follow/follow_iph_coordinator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_mediator.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/lens/lens_coordinator.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/default_browser_scene_agent.h"
#import "ios/chrome/browser/ui/main/layout_guide_util.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"
#import "ios/chrome/browser/ui/open_in/features.h"
#import "ios/chrome/browser/ui/open_in/open_in_coordinator.h"
#import "ios/chrome/browser/ui/overlays/overlay_container_coordinator.h"
#import "ios/chrome/browser/ui/page_info/page_info_coordinator.h"
#import "ios/chrome/browser/ui/passwords/password_breach_coordinator.h"
#import "ios/chrome/browser/ui/passwords/password_protection_coordinator.h"
#import "ios/chrome/browser/ui/passwords/password_suggestion_coordinator.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_coordinator.h"
#import "ios/chrome/browser/ui/presenters/vertical_animation_container.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_iph_coordinator.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_view_coordinator.h"
#import "ios/chrome/browser/ui/print/print_controller.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_coordinator.h"
#import "ios/chrome/browser/ui/qr_generator/qr_generator_coordinator.h"
#import "ios/chrome/browser/ui/qr_scanner/qr_scanner_legacy_coordinator.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_coordinator.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"
#import "ios/chrome/browser/ui/sad_tab/sad_tab_coordinator.h"
#import "ios/chrome/browser/ui/safe_browsing/safe_browsing_coordinator.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_coordinator.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator_delegate.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_coordinator.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_legacy_coordinator.h"
#import "ios/chrome/browser/ui/text_fragments/text_fragments_coordinator.h"
#import "ios/chrome/browser/ui/text_zoom/text_zoom_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_presenter.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_coordinating.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator_adaptor.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/page_animation_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/voice/text_to_speech_playback_controller.h"
#import "ios/chrome/browser/ui/voice/text_to_speech_playback_controller_factory.h"
#import "ios/chrome/browser/ui/webui/net_export_coordinator.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_coordinator.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web/annotations/annotations_tab_helper.h"
#import "ios/chrome/browser/web/font_size/font_size_tab_helper.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/print/print_tab_helper.h"
#import "ios/chrome/browser/web/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/repost_form_tab_helper_delegate.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/web_state_delegate_browser_agent.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/web_state_list/view_source_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/browser/webui/net_export_tab_helper_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Duration of the toolbar animation.
constexpr base::TimeDelta kLegacyFullscreenControllerToolbarAnimationDuration =
    base::Milliseconds(300);

// URL to share when user selects "Share Chrome"
const char kChromeAppStoreUrl[] = "https://google.com/chrome/go-mobile";

// Enum for toolbar to present.
enum class ToolbarKind {
  kTextZoom,
  kFindInPage,
};

}  // anonymous namespace

@interface BrowserCoordinator () <BrowserCoordinatorCommands,
                                  CRWWebStateObserver,
                                  DefaultBrowserPromoCommands,
                                  DefaultPromoNonModalPresentationDelegate,
                                  EnterprisePromptCoordinatorDelegate,
                                  FormInputAccessoryCoordinatorNavigator,
                                  NetExportTabHelperDelegate,
                                  NewTabPageCommands,
                                  PageInfoCommands,
                                  PageInfoPresentation,
                                  PasswordBreachCommands,
                                  PasswordProtectionCommands,
                                  PasswordSettingsCoordinatorDelegate,
                                  PasswordSuggestionCommands,
                                  PasswordSuggestionCoordinatorDelegate,
                                  PriceNotificationsCommands,
                                  PromosManagerCommands,
                                  PolicyChangeCommands,
                                  PreloadControllerDelegate,
                                  RepostFormTabHelperDelegate,
                                  SigninPresenter,
                                  SnapshotGeneratorDelegate,
                                  ToolbarAccessoryCoordinatorDelegate,
                                  URLLoadingDelegate,
                                  WebContentCommands,
                                  WebStateListObserving,
                                  WebNavigationNTPDelegate>

// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;

// Handles command dispatching, provided by the Browser instance.
@property(nonatomic, weak) CommandDispatcher* dispatcher;

// The coordinator managing the container view controller.
@property(nonatomic, strong)
    BrowserContainerCoordinator* browserContainerCoordinator;

// Coordinator between OpenIn TabHelper and OpenIn UI.
@property(nonatomic, strong) OpenInCoordinator* openInCoordinator;

// Mediator for incognito reauth.
@property(nonatomic, strong) IncognitoReauthMediator* incognitoAuthMediator;

// Mediator for tab lifecylce.
@property(nonatomic, strong) TabLifecycleMediator* tabLifecycleMediator;

// Mediator for tab events.
@property(nonatomic, strong) TabEventsMediator* tabEventsMediator;

// =================================================
// Child Coordinators, listed in alphabetical order.
// =================================================

// Coordinator for displaying a modal overlay with activity indicator to prevent
// the user from interacting with the browser view.
@property(nonatomic, strong)
    ActivityOverlayCoordinator* activityOverlayCoordinator;

// Presents a QLPreviewController in order to display USDZ format 3D models.
@property(nonatomic, strong) ARQuickLookCoordinator* ARQuickLookCoordinator;

// Coordinator to add new credit card.
@property(nonatomic, strong)
    AutofillAddCreditCardCoordinator* addCreditCardCoordinator;

// Coordinator for the badge popup menu.
@property(nonatomic, strong)
    BadgePopupMenuCoordinator* badgePopupMenuCoordinator;

// Coordinator-ish provider for context menus.
@property(nonatomic, strong)
    ContextMenuConfigurationProvider* contextMenuProvider;

// Coordinator for the find bar.
@property(nonatomic, strong) FindBarCoordinator* findBarCoordinator;

// Coordinator for the First Follow modal.
@property(nonatomic, strong) FirstFollowCoordinator* firstFollowCoordinator;

// Coordinator for the Follow IPH feature.
@property(nonatomic, strong) FollowIPHCoordinator* followIPHCoordinator;

// Coordinator in charge of the presenting autofill options above the
// keyboard.
@property(nonatomic, strong)
    FormInputAccessoryCoordinator* formInputAccessoryCoordinator;

// Presents a SFSafariViewController in order to download .mobileconfig file.
@property(nonatomic, strong)
    SafariDownloadCoordinator* SafariDownloadCoordinator;

// Opens downloaded Vcard.
@property(nonatomic, strong) VcardCoordinator* vcardCoordinator;

// The coordinator that manages net export.
@property(nonatomic, strong) NetExportCoordinator* netExportCoordinator;

// Coordinator for Page Info UI.
@property(nonatomic, strong) ChromeCoordinator* pageInfoCoordinator;

// Coordinator for the PassKit UI presentation.
@property(nonatomic, strong) PassKitCoordinator* passKitCoordinator;

// Coordinator for the password breach UI presentation.
@property(nonatomic, strong)
    PasswordBreachCoordinator* passwordBreachCoordinator;

// Coordinator for the password protection UI presentation.
@property(nonatomic, strong)
    PasswordProtectionCoordinator* passwordProtectionCoordinator;

// Coordinator for the password settings UI presentation.
@property(nonatomic, strong)
    PasswordSettingsCoordinator* passwordSettingsCoordinator;

// Coordinator for the password suggestion UI presentation.
@property(nonatomic, strong)
    PasswordSuggestionCoordinator* passwordSuggestionCoordinator;

// Coordinator for the price notifications IPH feature.
@property(nonatomic, strong)
    PriceNotificationsIPHCoordinator* priceNotificationsIPHCoordinator;

// Coordinator for the price notifications UI presentation.
@property(nonatomic, strong)
    PriceNotificationsViewCoordinator* priceNotificationsViewCoordiantor;

// Used to display the Print UI. Nil if not visible.
// TODO(crbug.com/910017): Convert to coordinator.
@property(nonatomic, strong) PrintController* printController;

// Coordinator for app-wide promos.
@property(nonatomic, strong) PromosManagerCoordinator* promosManagerCoordinator;

// Coordinator for the QR scanner.
@property(nonatomic, strong) QRScannerLegacyCoordinator* qrScannerCoordinator;

// Coordinator for the popup menu.
@property(nonatomic, strong) PopupMenuCoordinator* popupMenuCoordinator;

// Coordinator for displaying the Reading List.
@property(nonatomic, strong) ReadingListCoordinator* readingListCoordinator;

// Coordinator for Recent Tabs.
@property(nonatomic, strong) RecentTabsCoordinator* recentTabsCoordinator;

// Coordinator for displaying Repost Form dialog.
@property(nonatomic, strong) RepostFormCoordinator* repostFormCoordinator;

// Coordinator for displaying Sad Tab.
@property(nonatomic, strong) SadTabCoordinator* sadTabCoordinator;

// Coordinator for Safe Browsing.
@property(nonatomic, strong) SafeBrowsingCoordinator* safeBrowsingCoordinator;

// Coordinator for sharing scenarios.
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;

// Coordinator for presenting SKStoreProductViewController.
@property(nonatomic, strong) StoreKitCoordinator* storeKitCoordinator;

// Coordinator for Text Zoom.
@property(nonatomic, strong) TextZoomCoordinator* textZoomCoordinator;

// Coordinator that manages the default browser promo modal.
@property(nonatomic, strong)
    DefaultBrowserPromoCoordinator* defaultBrowserPromoCoordinator;

// Coordinator that manages the presentation of Download Manager UI.
@property(nonatomic, strong)
    DownloadManagerCoordinator* downloadManagerCoordinator;

// Coordinator that manages the tailored promo modals.
@property(nonatomic, strong) TailoredPromoCoordinator* tailoredPromoCoordinator;

// The container coordinators for the infobar modalities.
@property(nonatomic, strong)
    OverlayContainerCoordinator* infobarBannerOverlayContainerCoordinator;
@property(nonatomic, strong)
    OverlayContainerCoordinator* infobarModalOverlayContainerCoordinator;

// Coordinator for the non-modal default promo.
@property(nonatomic, strong)
    DefaultBrowserPromoNonModalCoordinator* nonModalPromoCoordinator;

// The coordinator that manages enterprise prompts.
@property(nonatomic, strong)
    EnterprisePromptCoordinator* enterprisePromptCoordinator;

// The coordinator used for the Text Fragments feature.
@property(nonatomic, strong) TextFragmentsCoordinator* textFragmentsCoordinator;

// The coordinator used for What's New feature.
@property(nonatomic, strong) WhatsNewCoordinator* whatsNewCoordinator;

@end

@implementation BrowserCoordinator {
  // Observers for WebStateList.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<base::ScopedObservation<WebStateList, WebStateListObserver>>
      _scopedWebStateListObservation;
  std::unique_ptr<
      base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>>
      _scopedWebStatesObservation;
  BrowserViewControllerDependencies _viewControllerDependencies;
  KeyCommandsProvider* _keyCommandsProvider;
  PrerenderService* _prerenderService;
  BubblePresenter* _bubblePresenter;
  ToolbarAccessoryPresenter* _toolbarAccessoryPresenter;
  NewTabPageCoordinator* _ntpCoordinator;
  LensCoordinator* _lensCoordinator;
  ToolbarCoordinatorAdaptor* _toolbarCoordinatorAdaptor;
  PrimaryToolbarCoordinator* _primaryToolbarCoordinator;
  SecondaryToolbarCoordinator* _secondaryToolbarCoordinator;
  TabStripCoordinator* _tabStripCoordinator;
  TabStripLegacyCoordinator* _legacyTabStripCoordinator;
  SideSwipeController* _sideSwipeController;
  FullscreenController* _fullscreenController;
  // The coordinator that shows the Send Tab To Self UI.
  SendTabToSelfCoordinator* _sendTabToSelfCoordinator;
  BookmarkInteractionController* _bookmarkInteractionController;
  id<TextZoomCommands> _textZoomHandler;
  id<HelpCommands> _helpHandler;
  id<PopupMenuCommands> _popupMenuCommandsHandler;
  id<SnackbarCommands> _snackbarCommandsHandler;
  absl::optional<ToolbarKind> _nextToolbarToPresent;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (self.started)
    return;

  DCHECK(!self.viewController);

  [self addWebStateObserver];
  [self addWebStateListObserver];
  [self createViewControllerDependencies];
  [self createViewController];
  [self updateViewControllerDependencies];
  // Mediators should start before coordinators so model state is accurate for
  // any UI that starts up.
  [self startMediators];
  [self installDelegatesForAllWebStates];
  [self startChildCoordinators];
  // Browser delegates can have dependencies on coordinators.
  [self installDelegatesForBrowser];
  [self installDelegatesForBrowserState];
  [super start];
  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;
  [super stop];

  self.active = NO;
  [self uninstallDelegatesForBrowserState];
  [self uninstallDelegatesForBrowser];
  [self uninstallDelegatesForAllWebStates];
  [self.tabEventsMediator disconnect];
  [self.tabLifecycleMediator disconnect];
  self.viewController.commandDispatcher = nil;
  [self.dispatcher stopDispatchingToTarget:self];
  [self stopChildCoordinators];
  [self destroyViewController];
  [self destroyViewControllerDependencies];
  [self removeWebStateListObserver];
  [self removeWebStateObserver];
  self.started = NO;
}

#pragma mark - Public

- (void)setActive:(BOOL)active {
  DCHECK_EQ(_active, self.viewController.active);
  if (_active == active) {
    return;
  }
  _active = active;

  // If not active, display an activity indicator overlay over the view to
  // prevent interaction with the web page.
  if (active) {
    [self hideActivityOverlay];
  } else if (!self.activityOverlayCoordinator) {
    [self showActivityOverlay];
  }

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  if (browserState) {
    // TODO(crbug.com/1272520): Refactor ActiveStateManager for multiwindow.
    ActiveStateManager* active_state_manager =
        ActiveStateManager::FromBrowserState(browserState);
    active_state_manager->SetActive(active);

    TextToSpeechPlaybackControllerFactory::GetInstance()
        ->GetForBrowserState(browserState)
        ->SetEnabled(active);
  }

  // TODO(crbug.com/1272516): Update the WebUsageEnablerBrowserAgent as part of
  // setting active/inactive.
  self.viewController.active = active;
}

- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  [self.passKitCoordinator stop];

  [self.openInCoordinator dismissAll];

  [self.printController dismissAnimated:YES];

  [self.readingListCoordinator stop];
  self.readingListCoordinator = nil;

  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;

  [self.passwordBreachCoordinator stop];
  self.passwordBreachCoordinator = nil;

  [self.passwordProtectionCoordinator stop];
  self.passwordProtectionCoordinator = nil;

  [self.passwordSuggestionCoordinator stop];
  self.passwordSuggestionCoordinator = nil;

  [self.pageInfoCoordinator stop];

  [_sendTabToSelfCoordinator stop];
  _sendTabToSelfCoordinator = nil;

  [self.passwordSettingsCoordinator stop];
  self.passwordSettingsCoordinator.delegate = nil;
  self.passwordSettingsCoordinator = nil;

  [self.priceNotificationsViewCoordiantor stop];
  self.priceNotificationsViewCoordiantor = nil;

  [self.viewController clearPresentedStateWithCompletion:completion
                                          dismissOmnibox:dismissOmnibox];
}

- (void)displayPopupMenuWithBadgeItems:(NSArray<id<BadgeItem>>*)badgeItems {
  self.badgePopupMenuCoordinator = [[BadgePopupMenuCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.badgePopupMenuCoordinator setBadgeItemsToShow:badgeItems];
  [self.badgePopupMenuCoordinator start];
}

#pragma mark - Private

// Displays activity overlay.
- (void)showActivityOverlay {
  self.activityOverlayCoordinator = [[ActivityOverlayCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.activityOverlayCoordinator start];
}

// Hides activity overlay.
- (void)hideActivityOverlay {
  [self.activityOverlayCoordinator stop];
  self.activityOverlayCoordinator = nil;
}

// Shows a default promo with the passed type or nothing if a tailored promo is
// already present.
- (void)showTailoredPromoWithType:(DefaultPromoType)type {
  if (self.tailoredPromoCoordinator) {
    // Another promo is being shown, return early.
    return;
  }
  self.tailoredPromoCoordinator = [[TailoredPromoCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                            type:type];
  self.tailoredPromoCoordinator.handler = self;
  [self.tailoredPromoCoordinator start];
}

// Instantiates a BrowserViewController.
- (void)createViewController {
  DCHECK(self.browserContainerCoordinator.viewController);

  _viewController = [[BrowserViewController alloc]
                     initWithBrowser:self.browser
      browserContainerViewController:self.browserContainerCoordinator
                                         .viewController
                          dispatcher:self.dispatcher
                 keyCommandsProvider:_keyCommandsProvider
                        dependencies:_viewControllerDependencies];

  WebNavigationBrowserAgent::FromBrowser(self.browser)->SetDelegate(self);

  self.contextMenuProvider = [[ContextMenuConfigurationProvider alloc]
         initWithBrowser:self.browser
      baseViewController:_viewController];
}

// Shuts down the BrowserViewController.
- (void)destroyViewController {
  // TODO(crbug.com/1272516): Set the WebUsageEnablerBrowserAgent to disabled.
  self.viewController.active = NO;
  [self.viewController shutdown];
  _viewController = nil;
}

// Creates the browser view controller dependencies.
- (void)createViewControllerDependencies {
  _dispatcher = self.browser->GetCommandDispatcher();

  // Add commands protocols handled by this class in this array to let the
  // dispatcher know where to dispatch such commands. This must be done before
  // starting any child coordinator, otherwise they won't be able to resolve
  // handlers.
  NSArray<Protocol*>* protocols = @[
    @protocol(ActivityServiceCommands),
    @protocol(BrowserCoordinatorCommands),
    @protocol(DefaultPromoCommands),
    @protocol(DefaultBrowserPromoNonModalCommands),
    @protocol(FeedCommands),
    @protocol(PromosManagerCommands),
    @protocol(FindInPageCommands),
    @protocol(NewTabPageCommands),
    @protocol(PageInfoCommands),
    @protocol(PasswordBreachCommands),
    @protocol(PasswordProtectionCommands),
    @protocol(PasswordSuggestionCommands),
    @protocol(PolicyChangeCommands),
    @protocol(PriceNotificationsCommands),
    @protocol(TextZoomCommands),
    @protocol(WebContentCommands),
  ];

  for (Protocol* protocol in protocols) {
    [_dispatcher startDispatchingToTarget:self forProtocol:protocol];
  }

  ChromeBrowserState* browserState = self.browser->GetBrowserState();

  _keyCommandsProvider =
      [[KeyCommandsProvider alloc] initWithBrowser:self.browser];
  _keyCommandsProvider.dispatcher =
      static_cast<id<ApplicationCommands, BrowserCommands, FindInPageCommands>>(
          _dispatcher);
  _keyCommandsProvider.omniboxHandler =
      static_cast<id<OmniboxCommands>>(_dispatcher);
  _keyCommandsProvider.bookmarksCommandsHandler =
      static_cast<id<BookmarksCommands>>(_dispatcher);
  _keyCommandsProvider.browserCoordinatorCommandsHandler =
      HandlerForProtocol(_dispatcher, BrowserCoordinatorCommands);

  _prerenderService = PrerenderServiceFactory::GetForBrowserState(browserState);
  if (!browserState->IsOffTheRecord()) {
    DCHECK(_prerenderService);
    _prerenderService->SetDelegate(self);
  }

  _fullscreenController = FullscreenController::FromBrowser(self.browser);

  _primaryToolbarCoordinator =
      [[PrimaryToolbarCoordinator alloc] initWithBrowser:self.browser];

  _secondaryToolbarCoordinator =
      [[SecondaryToolbarCoordinator alloc] initWithBrowser:self.browser];

  _toolbarCoordinatorAdaptor =
      [[ToolbarCoordinatorAdaptor alloc] initWithDispatcher:_dispatcher];

  [_toolbarCoordinatorAdaptor addToolbarCoordinator:_primaryToolbarCoordinator];
  [_toolbarCoordinatorAdaptor
      addToolbarCoordinator:_secondaryToolbarCoordinator];

  _bubblePresenter =
      [[BubblePresenter alloc] initWithBrowserState:browserState];
  _bubblePresenter.toolbarHandler =
      HandlerForProtocol(_dispatcher, ToolbarCommands);
  _bubblePresenter.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  [_dispatcher startDispatchingToTarget:_bubblePresenter
                            forProtocol:@protocol(HelpCommands)];

  _toolbarAccessoryPresenter = [[ToolbarAccessoryPresenter alloc]
      initWithIsIncognito:self.browser->GetBrowserState()->IsOffTheRecord()];

  _sideSwipeController =
      [[SideSwipeController alloc] initWithBrowser:self.browser];
  [_sideSwipeController setSnapshotDelegate:self];
  _sideSwipeController.toolbarInteractionHandler = _toolbarCoordinatorAdaptor;
  _sideSwipeController.primaryToolbarSnapshotProvider =
      _primaryToolbarCoordinator;
  _sideSwipeController.secondaryToolbarSnapshotProvider =
      _secondaryToolbarCoordinator;

  _bookmarkInteractionController =
      [[BookmarkInteractionController alloc] initWithBrowser:self.browser];

  self.browserContainerCoordinator = [[BrowserContainerCoordinator alloc]
      initWithBaseViewController:nil
                         browser:self.browser];
  [self.browserContainerCoordinator start];

  self.downloadManagerCoordinator = [[DownloadManagerCoordinator alloc]
      initWithBaseViewController:self.browserContainerCoordinator.viewController
                         browser:self.browser];
  self.downloadManagerCoordinator.presenter =
      [[VerticalAnimationContainer alloc] init];

  self.qrScannerCoordinator =
      [[QRScannerLegacyCoordinator alloc] initWithBrowser:self.browser];

  self.popupMenuCoordinator =
      [[PopupMenuCoordinator alloc] initWithBrowser:self.browser];
  self.popupMenuCoordinator.bubblePresenter = _bubblePresenter;
  self.popupMenuCoordinator.UIUpdater = _toolbarCoordinatorAdaptor;
  // Coordinator `start` is executed before setting it's `baseViewController`.
  // It is done intentionally, since this does not affecting the coordinator's
  // behavior but helps command handler setup below.
  [self.popupMenuCoordinator start];

  _primaryToolbarCoordinator.longPressDelegate = self.popupMenuCoordinator;
  _secondaryToolbarCoordinator.longPressDelegate = self.popupMenuCoordinator;

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    if (base::FeatureList::IsEnabled(kModernTabStrip)) {
      _tabStripCoordinator =
          [[TabStripCoordinator alloc] initWithBrowser:self.browser];
    } else {
      _legacyTabStripCoordinator =
          [[TabStripLegacyCoordinator alloc] initWithBrowser:self.browser];
      _legacyTabStripCoordinator.longPressDelegate = self.popupMenuCoordinator;
      _legacyTabStripCoordinator.animationWaitDuration =
          kLegacyFullscreenControllerToolbarAnimationDuration.InSecondsF();

      [_sideSwipeController setTabStripDelegate:_legacyTabStripCoordinator];
    }
  }

  _ntpCoordinator =
      [[NewTabPageCoordinator alloc] initWithBrowser:self.browser];
  _ntpCoordinator.toolbarDelegate = _toolbarCoordinatorAdaptor;
  _ntpCoordinator.bubblePresenter = _bubblePresenter;

  _lensCoordinator = [[LensCoordinator alloc] initWithBrowser:self.browser];

  _textZoomHandler = HandlerForProtocol(_dispatcher, TextZoomCommands);
  _helpHandler = HandlerForProtocol(_dispatcher, HelpCommands);
  _popupMenuCommandsHandler =
      HandlerForProtocol(_dispatcher, PopupMenuCommands);

  // SnackbarCoordinator is not created yet and therefore not dispatching
  // SnackbarCommands.
  _snackbarCommandsHandler = static_cast<id<SnackbarCommands>>(_dispatcher);

  _viewControllerDependencies.prerenderService = _prerenderService;
  _viewControllerDependencies.bubblePresenter = _bubblePresenter;
  _viewControllerDependencies.toolbarAccessoryPresenter =
      _toolbarAccessoryPresenter;
  _viewControllerDependencies.popupMenuCoordinator = self.popupMenuCoordinator;
  _viewControllerDependencies.downloadManagerCoordinator =
      self.downloadManagerCoordinator;
  _viewControllerDependencies.ntpCoordinator = _ntpCoordinator;
  _viewControllerDependencies.lensCoordinator = _lensCoordinator;
  _viewControllerDependencies.primaryToolbarCoordinator =
      _primaryToolbarCoordinator;
  _viewControllerDependencies.secondaryToolbarCoordinator =
      _secondaryToolbarCoordinator;
  _viewControllerDependencies.tabStripCoordinator = _tabStripCoordinator;
  _viewControllerDependencies.legacyTabStripCoordinator =
      _legacyTabStripCoordinator;
  _viewControllerDependencies.sideSwipeController = _sideSwipeController;
  _viewControllerDependencies.bookmarkInteractionController =
      _bookmarkInteractionController;
  _viewControllerDependencies.fullscreenController = _fullscreenController;
  _viewControllerDependencies.textZoomHandler = _textZoomHandler;
  _viewControllerDependencies.helpHandler = _helpHandler;
  _viewControllerDependencies.popupMenuCommandsHandler =
      _popupMenuCommandsHandler;
  _viewControllerDependencies.snackbarCommandsHandler =
      _snackbarCommandsHandler;
}

- (void)updateViewControllerDependencies {
  _bookmarkInteractionController.parentController = self.viewController;

  _bubblePresenter.delegate = self.viewController;
  _bubblePresenter.rootViewController = self.viewController;

  _toolbarAccessoryPresenter.baseViewController = self.viewController;

  self.qrScannerCoordinator.baseViewController = self.viewController;
  [self.qrScannerCoordinator start];

  self.popupMenuCoordinator.baseViewController = self.viewController;

  // The Lens coordinator needs to be started before the primary toolbar
  // coordinator so that the LensCommands dispatcher is correctly registered in
  // time.
  _lensCoordinator.baseViewController = self.viewController;
  [_lensCoordinator start];

  _primaryToolbarCoordinator.delegate = self.viewController;
  _primaryToolbarCoordinator.popupPresenterDelegate = self.viewController;
  [_primaryToolbarCoordinator start];

  _ntpCoordinator.baseViewController = self.viewController;

  [_dispatcher startDispatchingToTarget:self.viewController
                            forProtocol:@protocol(BrowserCommands)];
}

// Destroys the browser view controller dependencies.
- (void)destroyViewControllerDependencies {
  _viewControllerDependencies.prerenderService = nil;
  _viewControllerDependencies.bubblePresenter = nil;
  _viewControllerDependencies.toolbarAccessoryPresenter = nil;
  _viewControllerDependencies.popupMenuCoordinator = nil;
  _viewControllerDependencies.downloadManagerCoordinator = nil;
  _viewControllerDependencies.ntpCoordinator = nil;
  _viewControllerDependencies.lensCoordinator = nil;
  _viewControllerDependencies.primaryToolbarCoordinator = nil;
  _viewControllerDependencies.secondaryToolbarCoordinator = nil;
  _viewControllerDependencies.tabStripCoordinator = nil;
  _viewControllerDependencies.legacyTabStripCoordinator = nil;
  _viewControllerDependencies.sideSwipeController = nil;
  _viewControllerDependencies.textZoomHandler = nil;
  _viewControllerDependencies.helpHandler = nil;
  _viewControllerDependencies.bookmarkInteractionController = nil;

  [_bookmarkInteractionController shutdown];
  _bookmarkInteractionController = nil;

  _textZoomHandler = nil;
  _helpHandler = nil;
  _legacyTabStripCoordinator = nil;
  _tabStripCoordinator = nil;
  _sideSwipeController = nil;
  _toolbarCoordinatorAdaptor = nil;
  _secondaryToolbarCoordinator = nil;
  _primaryToolbarCoordinator = nil;

  [_dispatcher stopDispatchingToTarget:_bubblePresenter];
  [_bubblePresenter stop];
  _bubblePresenter = nil;
  _toolbarAccessoryPresenter = nil;

  _prerenderService = nil;
  _fullscreenController = nullptr;

  [self.popupMenuCoordinator stop];
  self.popupMenuCoordinator = nil;

  [self.qrScannerCoordinator stop];
  self.qrScannerCoordinator = nil;

  [_lensCoordinator stop];
  _lensCoordinator = nil;

  [self.downloadManagerCoordinator stop];
  self.downloadManagerCoordinator = nil;

  [self.browserContainerCoordinator stop];
  self.browserContainerCoordinator = nil;

  [_ntpCoordinator stop];
  _ntpCoordinator = nil;

  _keyCommandsProvider = nil;
  _dispatcher = nil;
}

// Starts child coordinators.
- (void)startChildCoordinators {
  // Dispatcher should be instantiated so that it can be passed to child
  // coordinators.
  DCHECK(self.dispatcher);

  self.ARQuickLookCoordinator = [[ARQuickLookCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.ARQuickLookCoordinator start];

  if (IsWebChannelsEnabled()) {
    self.followIPHCoordinator = [[FollowIPHCoordinator alloc]
        initWithBaseViewController:self.viewController
                           browser:self.browser];
    [self.followIPHCoordinator start];
  }

  self.SafariDownloadCoordinator = [[SafariDownloadCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.SafariDownloadCoordinator start];

  self.vcardCoordinator =
      [[VcardCoordinator alloc] initWithBaseViewController:self.viewController
                                                   browser:self.browser];
  [self.vcardCoordinator start];

  self.printController =
      [[PrintController alloc] initWithBaseViewController:self.viewController];

  // Help should only show in regular, non-incognito.
  if (!self.browser->GetBrowserState()->IsOffTheRecord()) {
    [self.popupMenuCoordinator startPopupMenuHelpCoordinator];
  }

  /* NetExportCoordinator is created and started by a delegate method */

  /* passwordBreachCoordinator is created and started by a BrowserCommand */

  /* passwordProtectionCoordinator is created and started by a BrowserCommand */

  /* passwordSettingsCoordinator is created and started by a delegate method */

  /* passwordSuggestionCoordinator is created and started by a BrowserCommand */

  /* PriceNotificationsViewCoordinator is created and started by a
   * BrowserCommand */

  /* ReadingListCoordinator is created and started by a BrowserCommand */

  /* RecentTabsCoordinator is created and started by a BrowserCommand */

  /* RepostFormCoordinator is created and started by a delegate method */

  /* WhatsNewCoordinator is created and started by a BrowserCommand */

  // TODO(crbug.com/1298934): Should start when the Sad Tab UI appears.
  self.sadTabCoordinator =
      [[SadTabCoordinator alloc] initWithBaseViewController:self.viewController
                                                    browser:self.browser];
  [self.sadTabCoordinator setOverscrollDelegate:self.viewController];

  /* SharingCoordinator is created and started by an ActivityServiceCommand */

  self.addCreditCardCoordinator = [[AutofillAddCreditCardCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];

  self.safeBrowsingCoordinator = [[SafeBrowsingCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.safeBrowsingCoordinator start];

  self.textFragmentsCoordinator = [[TextFragmentsCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.textFragmentsCoordinator start];

  // TODO(crbug.com/1334188): Refactor this coordinator so it doesn't directly
  // access the BVC's view.
  self.formInputAccessoryCoordinator = [[FormInputAccessoryCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.formInputAccessoryCoordinator.navigator = self;
  [self.formInputAccessoryCoordinator start];

  // TODO(crbug.com/1334188): Refactor this coordinator so it doesn't dirctly
  // access the BVC's view.
  self.infobarModalOverlayContainerCoordinator =
      [[OverlayContainerCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                            modality:OverlayModality::kInfobarModal];
  [self.infobarModalOverlayContainerCoordinator start];
  self.viewController.infobarModalOverlayContainerViewController =
      self.infobarModalOverlayContainerCoordinator.viewController;

  // TODO(crbug.com/1334188): Refactor this coordinator so it doesn't directly
  // access the BVC's view.
  self.infobarBannerOverlayContainerCoordinator =
      [[OverlayContainerCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                            modality:OverlayModality::kInfobarBanner];
  [self.infobarBannerOverlayContainerCoordinator start];
  self.viewController.infobarBannerOverlayContainerViewController =
      self.infobarBannerOverlayContainerCoordinator.viewController;

  if (IsPriceNotificationsEnabled()) {
    self.priceNotificationsIPHCoordinator =
        [[PriceNotificationsIPHCoordinator alloc]
            initWithBaseViewController:self.viewController
                               browser:self.browser];
    [self.priceNotificationsIPHCoordinator start];
  }
}

// Stops child coordinators.
- (void)stopChildCoordinators {
  [self.ARQuickLookCoordinator stop];
  self.ARQuickLookCoordinator = nil;

  [self.findBarCoordinator stop];
  self.findBarCoordinator = nil;

  [self.firstFollowCoordinator stop];
  self.firstFollowCoordinator = nil;

  [self.followIPHCoordinator stop];
  self.followIPHCoordinator = nil;

  [self.formInputAccessoryCoordinator stop];
  self.formInputAccessoryCoordinator = nil;

  [self.SafariDownloadCoordinator stop];
  self.SafariDownloadCoordinator = nil;

  [self.vcardCoordinator stop];
  self.vcardCoordinator = nil;

  [self.pageInfoCoordinator stop];
  self.pageInfoCoordinator = nil;

  [self.passKitCoordinator stop];
  self.passKitCoordinator = nil;

  [self.passwordBreachCoordinator stop];
  self.passwordBreachCoordinator = nil;

  [self.passwordProtectionCoordinator stop];
  self.passwordProtectionCoordinator = nil;

  [self.passwordSuggestionCoordinator stop];
  self.passwordSuggestionCoordinator = nil;

  self.printController = nil;

  [self.priceNotificationsViewCoordiantor stop];
  self.priceNotificationsViewCoordiantor = nil;

  if (IsFullscreenPromosManagerEnabled()) {
    [self.promosManagerCoordinator stop];
    self.promosManagerCoordinator = nil;
  }

  [self.readingListCoordinator stop];
  self.readingListCoordinator = nil;

  [self.recentTabsCoordinator stop];
  self.recentTabsCoordinator = nil;

  [self.repostFormCoordinator stop];
  self.repostFormCoordinator = nil;

  // TODO(crbug.com/1298934): Should stop when the Sad Tab UI appears.
  [self.sadTabCoordinator stop];
  [self.sadTabCoordinator disconnect];
  self.sadTabCoordinator = nil;

  [self.safeBrowsingCoordinator stop];
  self.safeBrowsingCoordinator = nil;

  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;

  [self.storeKitCoordinator stop];
  self.storeKitCoordinator = nil;

  [self.textZoomCoordinator stop];
  self.textZoomCoordinator = nil;

  [self.addCreditCardCoordinator stop];
  self.addCreditCardCoordinator = nil;

  [self.infobarBannerOverlayContainerCoordinator stop];
  self.infobarBannerOverlayContainerCoordinator = nil;

  [self.infobarModalOverlayContainerCoordinator stop];
  self.infobarModalOverlayContainerCoordinator = nil;

  [self.defaultBrowserPromoCoordinator stop];
  self.defaultBrowserPromoCoordinator = nil;

  [self.tailoredPromoCoordinator stop];
  self.tailoredPromoCoordinator = nil;

  [self.textFragmentsCoordinator stop];
  self.textFragmentsCoordinator = nil;

  [self.nonModalPromoCoordinator stop];
  self.nonModalPromoCoordinator = nil;

  [self.netExportCoordinator stop];
  self.netExportCoordinator = nil;

  [_sendTabToSelfCoordinator stop];
  _sendTabToSelfCoordinator = nil;

  [self.whatsNewCoordinator stop];
  self.whatsNewCoordinator = nil;

  [self.passwordSettingsCoordinator stop];
  self.passwordSettingsCoordinator.delegate = nil;
  self.passwordSettingsCoordinator = nil;

  [self.priceNotificationsIPHCoordinator stop];
  self.priceNotificationsIPHCoordinator = nil;
}

// Starts mediators owned by this coordinator.
- (void)startMediators {
  // Cache frequently repeated property values to curb generated code bloat.
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  BrowserViewController* browserViewController = self.viewController;

  TabLifecycleDependencies dependencies;
  dependencies.prerenderService =
      PrerenderServiceFactory::GetForBrowserState(browserState);
  dependencies.sideSwipeController = _sideSwipeController;
  dependencies.downloadManagerCoordinator = self.downloadManagerCoordinator;
  dependencies.baseViewController = browserViewController;
  dependencies.commandDispatcher = self.browser->GetCommandDispatcher();
  dependencies.tabHelperDelegate = self;

  self.tabLifecycleMediator = [[TabLifecycleMediator alloc]
           initWithWebStateList:self.browser->GetWebStateList()
                       delegate:browserViewController
      snapshotGeneratorDelegate:self
                   dependencies:dependencies];

  self.tabEventsMediator = [[TabEventsMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()
            ntpCoordinator:_ntpCoordinator];

  self.viewController.reauthHandler =
      HandlerForProtocol(self.dispatcher, IncognitoReauthCommands);

  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();

  self.viewController.nonModalPromoScheduler =
      [DefaultBrowserSceneAgent agentFromScene:sceneState].nonModalScheduler;
  self.viewController.nonModalPromoPresentationDelegate = self;

  if (browserState->IsOffTheRecord()) {
    IncognitoReauthSceneAgent* reauthAgent =
        [IncognitoReauthSceneAgent agentFromScene:sceneState];

    self.incognitoAuthMediator =
        [[IncognitoReauthMediator alloc] initWithConsumer:browserViewController
                                              reauthAgent:reauthAgent];
  }
}

#pragma mark - ActivityServiceCommands

- (void)sharePage {
  ActivityParams* params = [[ActivityParams alloc]
      initWithScenario:ActivityScenario::TabShareButton];

  // Exit fullscreen if needed to make sure that share button is visible.
  _fullscreenController->ExitFullscreen();

  id<ActivityServicePositioner> positioner =
      _primaryToolbarCoordinator.activityServicePositioner;
  UIBarButtonItem* anchor = nil;
  if ([positioner respondsToSelector:@selector(barButtonItem)]) {
    anchor = positioner.barButtonItem;
  }

  self.sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                          params:params
                      originView:positioner.sourceView
                      originRect:positioner.sourceRect
                          anchor:anchor];
  [self.sharingCoordinator start];
}

- (void)shareChromeApp {
  GURL URL = GURL(kChromeAppStoreUrl);
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_SHARE_CHROME_TITLE);
  NSString* additionalText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_SHARE_CHROME_DESC);
  ActivityParams* params =
      [[ActivityParams alloc] initWithURL:URL
                                    title:title
                           additionalText:additionalText
                                 scenario:ActivityScenario::ShareChrome];

  // Exit fullscreen if needed to make sure that share button is visible.
  FullscreenController::FromBrowser(self.browser)->ExitFullscreen();

  NamedGuide* guide = [NamedGuide guideWithName:kToolsMenuGuide
                                           view:self.viewController.view];
  self.sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                          params:params
                      originView:guide.constrainedView];
  [self.sharingCoordinator start];
}

- (void)shareHighlight:(ShareHighlightCommand*)command {
  ActivityParams* params =
      [[ActivityParams alloc] initWithURL:command.URL
                                    title:command.title
                           additionalText:command.selectedText
                                 scenario:ActivityScenario::SharedHighlight];

  self.sharingCoordinator =
      [[SharingCoordinator alloc] initWithBaseViewController:self.viewController
                                                     browser:self.browser
                                                      params:params
                                                  originView:command.sourceView
                                                  originRect:command.sourceRect
                                                      anchor:nil];
  [self.sharingCoordinator start];
}

#pragma mark - BrowserCoordinatorCommands

- (void)printTabWithBaseViewController:(UIViewController*)baseViewController {
  DCHECK(self.printController);
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  [self.printController printWebState:webState
                   baseViewController:baseViewController];
}

- (void)printImage:(UIImage*)image
                 title:(NSString*)title
    baseViewController:(UIViewController*)baseViewController {
  DCHECK(self.printController);
  [self.printController printImage:image
                             title:title
                baseViewController:baseViewController];
}

- (void)showReadingList {
  self.readingListCoordinator = [[ReadingListCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.readingListCoordinator start];
}

- (void)showBookmarksManager {
  [_bookmarkInteractionController presentBookmarks];
}

- (void)showReadingListIPH {
  [_bubblePresenter presentReadingListBottomToolbarTipBubble];
}

- (void)showFollowWhileBrowsingIPH {
  [_bubblePresenter presentFollowWhileBrowsingTipBubble];
}

- (void)showDefaultSiteViewIPH {
  [_bubblePresenter presentDefaultSiteViewTipBubble];
}

- (void)showDownloadsFolder {
  NSURL* URL = GetFilesAppUrl();
  if (!URL)
    return;

  [[UIApplication sharedApplication] openURL:URL
                                     options:@{}
                           completionHandler:nil];

  base::UmaHistogramEnumeration(
      "Download.OpenDownloads.PerProfileType",
      profile_metrics::GetBrowserProfileType(self.browser->GetBrowserState()));
}

- (void)showRecentTabs {
  // TODO(crbug.com/825431): If BVC's clearPresentedState is ever called (such
  // as in tearDown after a failed egtest), then this coordinator is left in a
  // started state even though its corresponding VC is no longer on screen.
  // That causes issues when the coordinator is started again and we destroy the
  // old mediator without disconnecting it first.  Temporarily work around these
  // issues by not having a long lived coordinator.  A longer-term solution will
  // require finding a way to stop this coordinator so that the mediator is
  // properly disconnected and destroyed and does not live longer than its
  // associated VC.
  if (self.recentTabsCoordinator) {
    [self.recentTabsCoordinator stop];
    self.recentTabsCoordinator = nil;
  }

  self.recentTabsCoordinator = [[RecentTabsCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.recentTabsCoordinator.loadStrategy = UrlLoadStrategy::NORMAL;
  [self.recentTabsCoordinator start];
}

- (void)showTranslate {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();

  feature_engagement::Tracker* engagement_tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(browserState);
  engagement_tracker->NotifyEvent(
      feature_engagement::events::kTriggeredTranslateInfobar);

  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  DCHECK(currentWebState);

  ChromeIOSTranslateClient* translateClient =
      ChromeIOSTranslateClient::FromWebState(currentWebState);
  if (translateClient) {
    translate::TranslateManager* translateManager =
        translateClient->GetTranslateManager();
    DCHECK(translateManager);
    translateManager->ShowTranslateUI(/*auto_translate=*/true);
  }
}

- (void)showHelpPage {
  GURL helpUrl(l10n_util::GetStringUTF16(IDS_IOS_TOOLS_MENU_HELP_URL));
  UrlLoadParams params = UrlLoadParams::InNewTab(helpUrl);
  params.append_to = kCurrentTab;
  params.user_initiated = NO;
  params.in_incognito = self.browser->GetBrowserState()->IsOffTheRecord();
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
}

- (void)showAddCreditCard {
  [self.addCreditCardCoordinator start];
}

- (void)showSendTabToSelfUI:(const GURL&)url title:(NSString*)title {
  // TODO(crbug.com/1347821): Make this DCHECK(!_sendTabToSelfCoordinator)
  // once SendTabToSelfCoordinator is aware of sign-in being aborted.
  if (_sendTabToSelfCoordinator) {
    [_sendTabToSelfCoordinator stop];
    _sendTabToSelfCoordinator = nil;
  }

  _sendTabToSelfCoordinator = [[SendTabToSelfCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                 signinPresenter:self
                             url:url
                           title:title];
  [_sendTabToSelfCoordinator start];
}

- (void)hideSendTabToSelfUI {
  DCHECK(_sendTabToSelfCoordinator);
  [_sendTabToSelfCoordinator stop];
  _sendTabToSelfCoordinator = nil;
}

- (void)dismissBadgePopupMenu {
  [self.badgePopupMenuCoordinator stop];
}

#if !defined(NDEBUG)
- (void)viewSource {
  ViewSourceBrowserAgent* viewSourceAgent =
      ViewSourceBrowserAgent::FromBrowser(self.browser);
  viewSourceAgent->ViewSourceForActiveWebState();
}
#endif  // !defined(NDEBUG)

- (void)focusFakebox {
  if ([self isNTPActiveForCurrentWebState]) {
    [_ntpCoordinator focusFakebox];
  }
}

// TODO(crbug.com/1272498): Refactor this command away, and add a mediator to
// observe the active web state closing and push updates into the BVC for UI
// work.
- (void)closeCurrentTab {
  WebStateList* webStateList = self.browser->GetWebStateList();

  int active_index = webStateList->active_index();
  if (active_index == WebStateList::kInvalidIndex)
    return;

  BOOL canShowTabStrip = IsRegularXRegularSizeClass(self.viewController);

  UIView* contentArea = self.browserContainerCoordinator.viewController.view;
  UIView* snapshotView = nil;

  if (!canShowTabStrip) {
    snapshotView = [contentArea snapshotViewAfterScreenUpdates:NO];
    snapshotView.frame = contentArea.frame;
  }

  webStateList->CloseWebStateAt(active_index, WebStateList::CLOSE_USER_ACTION);

  if (!canShowTabStrip) {
    [contentArea addSubview:snapshotView];
    page_animation_util::AnimateOutWithCompletion(snapshotView, ^{
      [snapshotView removeFromSuperview];
    });
  }
}

- (void)showWhatsNew {
  if (!IsWhatsNewEnabled()) {
    return;
  }

  self.whatsNewCoordinator = [[WhatsNewCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [self.whatsNewCoordinator start];
}

- (void)dismissWhatsNew {
  [self.whatsNewCoordinator stop];
  self.whatsNewCoordinator = nil;
}

- (void)showWhatsNewIPH {
  [_bubblePresenter presentWhatsNewBottomToolbarBubble];
}

#pragma mark - DefaultPromoCommands

- (void)showTailoredPromoStaySafe {
  [self showTailoredPromoWithType:DefaultPromoTypeStaySafe];
}

- (void)showTailoredPromoMadeForIOS {
  [self showTailoredPromoWithType:DefaultPromoTypeMadeForIOS];
}

- (void)showTailoredPromoAllTabs {
  [self showTailoredPromoWithType:DefaultPromoTypeAllTabs];
}

- (void)showDefaultBrowserFullscreenPromo {
  if (!self.defaultBrowserPromoCoordinator) {
    self.defaultBrowserPromoCoordinator =
        [[DefaultBrowserPromoCoordinator alloc]
            initWithBaseViewController:self.viewController
                               browser:self.browser];
    self.defaultBrowserPromoCoordinator.handler = self;
  }
  [self.defaultBrowserPromoCoordinator start];
}

#pragma mark - DefaultBrowserPromoCommands

- (void)hidePromo {
  [self.defaultBrowserPromoCoordinator stop];
  self.defaultBrowserPromoCoordinator = nil;
  [self.tailoredPromoCoordinator stop];
  self.tailoredPromoCoordinator = nil;
}

#pragma mark - FeedCommands

- (void)showFirstFollowUIForWebSite:(FollowedWebSite*)followedWebSite {
  self.firstFollowCoordinator = [[FirstFollowCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                 followedWebSite:followedWebSite];
  [self.firstFollowCoordinator start];
}

#pragma mark - FindInPageCommands

- (void)openFindInPage {
  if (!self.canShowFindBar)
    return;

  if (_toolbarAccessoryPresenter.isPresenting) {
    _nextToolbarToPresent = ToolbarKind::kFindInPage;
    [self closeTextZoom];
    return;
  }

  FindBarCoordinator* findBarCoordinator = self.findBarCoordinator;
  if (findBarCoordinator) {
    [findBarCoordinator stop];
    self.findBarCoordinator = nil;
  }

  findBarCoordinator =
      [[FindBarCoordinator alloc] initWithBaseViewController:self.viewController
                                                     browser:self.browser];
  self.findBarCoordinator = findBarCoordinator;

  findBarCoordinator.presenter = _toolbarAccessoryPresenter;
  findBarCoordinator.delegate = self;
  findBarCoordinator.presentationDelegate = self.viewController;
  [findBarCoordinator start];
}

- (void)closeFindInPage {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();

  if (currentWebState) {
    FindTabHelper* findTabHelper = FindTabHelper::FromWebState(currentWebState);
    if (findTabHelper->IsFindUIActive()) {
      findTabHelper->StopFinding();
    } else {
      [self.findBarCoordinator stop];
    }
  }
}

- (void)showFindUIIfActive {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  auto* findHelper = FindTabHelper::FromWebState(currentWebState);
  if (findHelper && findHelper->IsFindUIActive() &&
      !self.findBarCoordinator.presenter.isPresenting) {
    [self.findBarCoordinator start];
  }
}

- (void)hideFindUI {
  [self.findBarCoordinator stop];
}

- (void)defocusFindInPage {
  [self.findBarCoordinator defocusFindBar];
}

- (void)searchFindInPage {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  DCHECK(currentWebState);
  FindTabHelper* helper = FindTabHelper::FromWebState(currentWebState);
  helper->StartFinding([self.findBarCoordinator.findBarController searchTerm]);

  if (!self.browser->GetBrowserState()->IsOffTheRecord())
    helper->PersistSearchTerm();
}

- (void)findNextStringInPage {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  DCHECK(currentWebState);
  // TODO(crbug.com/603524): Reshow find bar if necessary.
  FindTabHelper::FromWebState(currentWebState)
      ->ContinueFinding(FindTabHelper::FORWARD);
}

- (void)findPreviousStringInPage {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  DCHECK(currentWebState);
  // TODO(crbug.com/603524): Reshow find bar if necessary.
  FindTabHelper::FromWebState(currentWebState)
      ->ContinueFinding(FindTabHelper::REVERSE);
}

#pragma mark - FindInPageCommands Helpers

- (BOOL)canShowFindBar {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return NO;
  }

  auto* helper = FindTabHelper::FromWebState(currentWebState);
  return (helper && helper->CurrentPageSupportsFindInPage() &&
          !helper->IsFindUIActive());
}

#pragma mark - PromosManagerCommands

- (void)maybeDisplayPromo {
  if (IsFullscreenPromosManagerEnabled()) {
    if (!self.promosManagerCoordinator) {
      self.promosManagerCoordinator = [[PromosManagerCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
    }

    [self.promosManagerCoordinator start];
  }
}

- (void)requestAppStoreReview {
  if (IsAppStoreRatingEnabled()) {
    UIWindowScene* scene =
        [SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState()
            scene];
    [SKStoreReviewController requestReviewInScene:scene];
  }
}

- (void)showWhatsNewPromo {
  [self showWhatsNew];
  self.whatsNewCoordinator.shouldShowBubblePromoOnDismiss = YES;
}

#pragma mark - PageInfoCommands

- (void)showPageInfo {
  PageInfoCoordinator* pageInfoCoordinator = [[PageInfoCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  pageInfoCoordinator.presentationProvider = self;
  self.pageInfoCoordinator = pageInfoCoordinator;
  [self.pageInfoCoordinator start];
}

- (void)hidePageInfo {
  [self.pageInfoCoordinator stop];
  self.pageInfoCoordinator = nil;
}

- (void)showSecurityHelpPage {
  UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kPageInfoHelpCenterURL));
  params.in_incognito = self.browser->GetBrowserState()->IsOffTheRecord();
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
  [self hidePageInfo];
}

#pragma mark - FormInputAccessoryCoordinatorNavigator

- (void)openPasswordSettings {
  // TODO(crbug.com/1361357) Remove call to
  // `showSavedPasswordsSettingsFromViewController` once `kIOSPasswordUISplit`
  // is on by default.
  if (base::FeatureList::IsEnabled(
          password_manager::features::kIOSPasswordUISplit)) {
    DCHECK(!self.passwordSettingsCoordinator);

    // Use main browser to open the password settings.
    SceneState* sceneState =
        SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
    self.passwordSettingsCoordinator = [[PasswordSettingsCoordinator alloc]
        initWithBaseViewController:self.viewController
                           browser:sceneState.interfaceProvider.mainInterface
                                       .browser];
    self.passwordSettingsCoordinator.delegate = self;
    [self.passwordSettingsCoordinator start];
  } else {
    [HandlerForProtocol(self.dispatcher, ApplicationCommands)
        showSavedPasswordsSettingsFromViewController:self.viewController
                                    showCancelButton:YES];
  }
}

- (void)openAddressSettings {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showProfileSettingsFromViewController:self.viewController];
}

- (void)openCreditCardSettings {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showCreditCardSettings];
}

#pragma mark - RepostFormTabHelperDelegate

- (void)repostFormTabHelper:(RepostFormTabHelper*)helper
    presentRepostFormDialogForWebState:(web::WebState*)webState
                         dialogAtPoint:(CGPoint)location
                     completionHandler:(void (^)(BOOL))completion {
  self.repostFormCoordinator = [[RepostFormCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                  dialogLocation:location
                        webState:webState
               completionHandler:completion];
  [self.repostFormCoordinator start];
}

- (void)repostFormTabHelperDismissRepostFormDialog:
    (RepostFormTabHelper*)helper {
  [self.repostFormCoordinator stop];
  self.repostFormCoordinator = nil;
}

#pragma mark - ToolbarAccessoryCoordinatorDelegate

- (void)toolbarAccessoryCoordinatorDidDismissUI:
    (ChromeCoordinator*)coordinator {
  if (self.findBarCoordinator) {
    [self.findBarCoordinator stop];
    self.findBarCoordinator = nil;
  }

  if (self.textZoomCoordinator) {
    [self.textZoomCoordinator stop];
    self.textZoomCoordinator = nil;
  }

  if (!_nextToolbarToPresent.has_value()) {
    return;
  }

  const ToolbarKind nextToolbarToPresent = *_nextToolbarToPresent;
  _nextToolbarToPresent = absl::nullopt;

  switch (nextToolbarToPresent) {
    case ToolbarKind::kTextZoom:
      [self openTextZoom];
      break;

    case ToolbarKind::kFindInPage:
      [self openFindInPage];
      break;
  }
}

#pragma mark - TextZoomCommands

- (void)openTextZoom {
  if (_toolbarAccessoryPresenter.isPresenting) {
    _nextToolbarToPresent = ToolbarKind::kTextZoom;
    [self closeFindInPage];
    return;
  }

  TextZoomCoordinator* textZoomCoordinator = self.textZoomCoordinator;
  if (textZoomCoordinator) {
    [textZoomCoordinator stop];
    self.textZoomCoordinator = nil;
  }

  textZoomCoordinator = [[TextZoomCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.textZoomCoordinator = textZoomCoordinator;

  textZoomCoordinator.presenter = _toolbarAccessoryPresenter;
  textZoomCoordinator.delegate = self;
  [textZoomCoordinator start];
}

- (void)closeTextZoom {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (currentWebState) {
    if (ios::provider::IsTextZoomEnabled()) {
      FontSizeTabHelper* fontSizeTabHelper =
          FontSizeTabHelper::FromWebState(currentWebState);
      fontSizeTabHelper->SetTextZoomUIActive(false);
    }
  }
  [self.textZoomCoordinator stop];
}

- (void)showTextZoomUIIfActive {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return;
  }

  FontSizeTabHelper* fontSizeTabHelper =
      FontSizeTabHelper::FromWebState(currentWebState);
  if (fontSizeTabHelper && fontSizeTabHelper->IsTextZoomUIActive() &&
      !self.textZoomCoordinator.presenter.isPresenting) {
    [self.textZoomCoordinator start];
  }
}

- (void)hideTextZoomUI {
  [self.textZoomCoordinator stop];
}

#pragma mark - URLLoadingServiceDelegate

- (void)animateOpenBackgroundTabFromParams:(const UrlLoadParams&)params
                                completion:(void (^)())completion {
  [self.viewController
      animateOpenBackgroundTabFromOriginPoint:params.origin_point
                                   completion:completion];
}

// TODO(crbug.com/906525) : Move WebStateListObserving out of
// BrowserCoordinator.
#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [self installDelegatesForWebState:webState];
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  [self uninstallDelegatesForWebState:oldWebState];
  [self installDelegatesForWebState:newWebState];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  [self uninstallDelegatesForWebState:webState];
}

// TODO(crbug.com/906525) : Move out of BrowserCoordinator along with
// WebStateListObserving.
#pragma mark - Private WebState management methods

// Adds observer for WebStateList.
- (void)addWebStateListObserver {
  _webStateListObserverBridge =
      std::make_unique<WebStateListObserverBridge>(self);
  _scopedWebStateListObservation = std::make_unique<
      base::ScopedObservation<WebStateList, WebStateListObserver>>(
      _webStateListObserverBridge.get());
  _scopedWebStateListObservation->Observe(self.browser->GetWebStateList());
}

// Removes observer for WebStateList.
- (void)removeWebStateListObserver {
  _scopedWebStateListObservation.reset();
  _webStateListObserverBridge.reset();
}

- (void)addWebStateObserver {
  _webStateObserverBridge = std::make_unique<web::WebStateObserverBridge>(self);
  _scopedWebStatesObservation = std::make_unique<
      base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>>(
      _webStateObserverBridge.get());
}

- (void)removeWebStateObserver {
  _scopedWebStatesObservation.reset();
}

// Installs delegates for each WebState in WebStateList.
- (void)installDelegatesForAllWebStates {
  if (!IsOpenInActivitiesInShareButtonEnabled()) {
    self.openInCoordinator = [[OpenInCoordinator alloc]
        initWithBaseViewController:self.viewController
                           browser:self.browser];
    [self.openInCoordinator start];
  }

  for (int i = 0; i < self.browser->GetWebStateList()->count(); i++) {
    web::WebState* webState = self.browser->GetWebStateList()->GetWebStateAt(i);
    [self installDelegatesForWebState:webState];
  }
}

// Installs delegates for self.browser.
- (void)installDelegatesForBrowser {
  // The view controller should have been created.
  DCHECK(self.viewController);

  SyncErrorBrowserAgent::FromBrowser(self.browser)->SetUIProviders(self, self);

  WebStateDelegateBrowserAgent::FromBrowser(self.browser)
      ->SetUIProviders(self.contextMenuProvider,
                       self.formInputAccessoryCoordinator, self.viewController);

  UrlLoadingBrowserAgent* loadingAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  if (loadingAgent) {
    loadingAgent->SetDelegate(self);
  }

  id<ApplicationCommands> applicationCommandHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  AccountConsistencyBrowserAgent::CreateForBrowser(
      self.browser, self.viewController, applicationCommandHandler);

  if (FollowBrowserAgent::FromBrowser(self.browser)) {
    CommandDispatcher* commandDispatcher = self.browser->GetCommandDispatcher();
    FollowBrowserAgent::FromBrowser(self.browser)
        ->SetUIProviders(
            HandlerForProtocol(commandDispatcher, NewTabPageCommands),
            static_cast<id<SnackbarCommands>>(commandDispatcher),
            HandlerForProtocol(commandDispatcher, FeedCommands));
  }
}

// Installs delegates for self.browser->GetBrowserState()
- (void)installDelegatesForBrowserState {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  if (browserState) {
    TextToSpeechPlaybackControllerFactory::GetInstance()
        ->GetForBrowserState(browserState)
        ->SetWebStateList(self.browser->GetWebStateList());
  }
}

// Uninstalls delegates for self.browser->GetBrowserState()
- (void)uninstallDelegatesForBrowserState {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  if (browserState) {
    TextToSpeechPlaybackControllerFactory::GetInstance()
        ->GetForBrowserState(browserState)
        ->SetWebStateList(nullptr);
  }
}

// Uninstalls delegates for self.browser.
- (void)uninstallDelegatesForBrowser {
  UrlLoadingBrowserAgent* loadingAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  if (loadingAgent) {
    loadingAgent->SetDelegate(nil);
  }

  WebStateDelegateBrowserAgent::FromBrowser(self.browser)->ClearUIProviders();

  SyncErrorBrowserAgent::FromBrowser(self.browser)->ClearUIProviders();

  if (FollowBrowserAgent::FromBrowser(self.browser)) {
    FollowBrowserAgent::FromBrowser(self.browser)->ClearUIProviders();
  }
}

// Uninstalls delegates for each WebState in WebStateList.
- (void)uninstallDelegatesForAllWebStates {
  if (!IsOpenInActivitiesInShareButtonEnabled()) {
    // OpenInCoordinator monitors the webStateList and should be stopped.
    [self.openInCoordinator stop];
    self.openInCoordinator = nil;
  }

  for (int i = 0; i < self.browser->GetWebStateList()->count(); i++) {
    web::WebState* webState = self.browser->GetWebStateList()->GetWebStateAt(i);
    [self uninstallDelegatesForWebState:webState];
  }
}

// Install delegates for `webState`.
- (void)installDelegatesForWebState:(web::WebState*)webState {
  if (!webState->IsRealized()) {
    [self startObservingRealizationForWebState:webState];
    return;
  }

  DCHECK(!_prerenderService ||
         !_prerenderService->IsWebStatePrerendered(webState));

  if (AutofillTabHelper::FromWebState(webState)) {
    AutofillTabHelper::FromWebState(webState)->SetBaseViewController(
        self.viewController);
  }

  if (PrintTabHelper::FromWebState(webState)) {
    PrintTabHelper::FromWebState(webState)->set_printer(self.printController);
  }

  RepostFormTabHelper::FromWebState(webState)->SetDelegate(self);

  FollowTabHelper* followTabHelper = FollowTabHelper::FromWebState(webState);
  if (followTabHelper) {
    followTabHelper->set_follow_iph_presenter(self.followIPHCoordinator);
  }

  if (CaptivePortalTabHelper::FromWebState(webState)) {
    TabInsertionBrowserAgent* insertionAgent =
        TabInsertionBrowserAgent::FromBrowser(self.browser);
    CaptivePortalTabHelper::FromWebState(webState)->SetTabInsertionBrowserAgent(
        insertionAgent);
  }

  if (NewTabPageTabHelper::FromWebState(webState)) {
    NewTabPageTabHelper::FromWebState(webState)->SetDelegate(
        self.viewController);
  }

  if (AnnotationsTabHelper::FromWebState(webState)) {
    AnnotationsTabHelper::FromWebState(webState)->SetBaseViewController(
        self.viewController);
  }

  PriceNotificationsTabHelper* priceNotificationsTabHelper =
      PriceNotificationsTabHelper::FromWebState(webState);
  if (priceNotificationsTabHelper) {
    priceNotificationsTabHelper->SetPriceNotificationsIPHPresenter(
        self.priceNotificationsIPHCoordinator);
  }
}

// Uninstalls delegates for `webState`.
- (void)uninstallDelegatesForWebState:(web::WebState*)webState {
  if (!webState->IsRealized()) {
    [self stopObservingRealizationForWebState:webState];
    return;
  }

  if (AutofillTabHelper::FromWebState(webState)) {
    AutofillTabHelper::FromWebState(webState)->SetBaseViewController(nil);
  }

  if (PrintTabHelper::FromWebState(webState)) {
    PrintTabHelper::FromWebState(webState)->set_printer(nil);
  }

  RepostFormTabHelper::FromWebState(webState)->SetDelegate(nil);

  FollowTabHelper* followTabHelper = FollowTabHelper::FromWebState(webState);
  if (followTabHelper) {
    followTabHelper->set_follow_iph_presenter(nil);
  }

  if (CaptivePortalTabHelper::FromWebState(webState)) {
    CaptivePortalTabHelper::FromWebState(webState)->SetTabInsertionBrowserAgent(
        nil);
  }

  if (NewTabPageTabHelper::FromWebState(webState)) {
    NewTabPageTabHelper::FromWebState(webState)->SetDelegate(nil);
  }

  if (AnnotationsTabHelper::FromWebState(webState)) {
    AnnotationsTabHelper::FromWebState(webState)->SetBaseViewController(nil);
  }

  PriceNotificationsTabHelper* priceNotificationsTabHelper =
      PriceNotificationsTabHelper::FromWebState(webState);
  if (priceNotificationsTabHelper) {
    priceNotificationsTabHelper->SetPriceNotificationsIPHPresenter(nil);
  }
}

- (void)startObservingRealizationForWebState:(web::WebState*)webState {
  if (_scopedWebStatesObservation->IsObservingSource(webState))
    return;
  _scopedWebStatesObservation->AddObservation(webState);
}

- (void)stopObservingRealizationForWebState:(web::WebState*)webState {
  _scopedWebStatesObservation->RemoveObservation(webState);
}

#pragma mark - CRWWebStateObserver

- (void)webStateRealized:(web::WebState*)webState {
  [self stopObservingRealizationForWebState:webState];
  [self installDelegatesForWebState:webState];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  [self stopObservingRealizationForWebState:webState];
}

#pragma mark - PasswordBreachCommands

- (void)showPasswordBreachForLeakType:(CredentialLeakType)leakType {
  self.passwordBreachCoordinator = [[PasswordBreachCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                        leakType:leakType];
  [self.passwordBreachCoordinator start];
}

#pragma mark - PasswordProtectionCommands

- (void)showPasswordProtectionWarning:(NSString*)warningText
                           completion:(void (^)(safe_browsing::WarningAction))
                                          completion {
  self.passwordProtectionCoordinator = [[PasswordProtectionCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                     warningText:warningText];
  [self.passwordProtectionCoordinator startWithCompletion:completion];
}

#pragma mark - PasswordSuggestionCommands

- (void)showPasswordSuggestion:(NSString*)passwordSuggestion
               decisionHandler:(void (^)(BOOL accept))decisionHandler {
  self.passwordSuggestionCoordinator = [[PasswordSuggestionCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
              passwordSuggestion:passwordSuggestion
                 decisionHandler:decisionHandler];
  self.passwordSuggestionCoordinator.delegate = self;
  [self.passwordSuggestionCoordinator start];
}

#pragma mark - PriceNotificationsCommands

- (void)showPriceNotifications {
  self.priceNotificationsViewCoordiantor =
      [[PriceNotificationsViewCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  [self.priceNotificationsViewCoordiantor start];
}

- (void)hidePriceNotifications {
  [self.priceNotificationsViewCoordiantor stop];
}

- (void)presentPriceNotificationsWhileBrowsingIPH {
  [_bubblePresenter presentPriceNotificationsWhileBrowsingTipBubble];
}

#pragma mark - PolicyChangeCommands

- (void)showForceSignedOutPrompt {
  if (!self.enterprisePromptCoordinator) {
    self.enterprisePromptCoordinator = [[EnterprisePromptCoordinator alloc]
        initWithBaseViewController:self.viewController
                           browser:self.browser
                        promptType:EnterprisePromptTypeForceSignOut];
    self.enterprisePromptCoordinator.delegate = self;
  }
  [self.enterprisePromptCoordinator start];
}

- (void)showSyncDisabledPrompt {
  if (!self.enterprisePromptCoordinator) {
    self.enterprisePromptCoordinator = [[EnterprisePromptCoordinator alloc]
        initWithBaseViewController:self.viewController
                           browser:self.browser
                        promptType:EnterprisePromptTypeSyncDisabled];
    self.enterprisePromptCoordinator.delegate = self;
  }
  [self.enterprisePromptCoordinator start];
}

- (void)showRestrictAccountSignedOutPrompt {
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  if (sceneState.activationLevel >= SceneActivationLevelForegroundActive) {
    if (!self.enterprisePromptCoordinator) {
      self.enterprisePromptCoordinator = [[EnterprisePromptCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                          promptType:
                              EnterprisePromptTypeRestrictAccountSignedOut];
      self.enterprisePromptCoordinator.delegate = self;
    }
    [self.enterprisePromptCoordinator start];
  } else {
    __weak BrowserCoordinator* weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 static_cast<int64_t>(1 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
                     [weakSelf showRestrictAccountSignedOutPrompt];
                   });
  }
}

#pragma mark - WebContentCommands

- (void)showAppStoreWithParameters:(NSDictionary*)productParameters {
  self.storeKitCoordinator = [[StoreKitCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.storeKitCoordinator.iTunesProductParameters = productParameters;
  [self.storeKitCoordinator start];
}

- (void)showDialogForPassKitPass:(PKPass*)pass {
  if (self.passKitCoordinator.pass) {
    // Another pass is being displayed -- early return (this is unexpected).
    return;
  }

  self.passKitCoordinator =
      [[PassKitCoordinator alloc] initWithBaseViewController:self.viewController
                                                     browser:self.browser];

  self.passKitCoordinator.pass = pass;
  [self.passKitCoordinator start];
}

#pragma mark - DefaultBrowserPromoNonModalCommands

- (void)showDefaultBrowserNonModalPromo {
  self.nonModalPromoCoordinator =
      [[DefaultBrowserPromoNonModalCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
  [self.nonModalPromoCoordinator start];
  self.nonModalPromoCoordinator.browser = self.browser;
  self.nonModalPromoCoordinator.baseViewController = self.viewController;
  [self.nonModalPromoCoordinator presentInfobarBannerAnimated:YES
                                                   completion:nil];
}

- (void)dismissDefaultBrowserNonModalPromoAnimated:(BOOL)animated {
  [self.nonModalPromoCoordinator dismissInfobarBannerAnimated:animated
                                                   completion:nil];
}

- (void)defaultBrowserNonModalPromoWasDismissed {
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  DefaultBrowserSceneAgent* agent =
      [DefaultBrowserSceneAgent agentFromScene:sceneState];
  [agent.nonModalScheduler logPromoWasDismissed];
  [self.nonModalPromoCoordinator stop];
  self.nonModalPromoCoordinator = nil;
}

#pragma mark - DefaultPromoNonModalPresentationDelegate

- (BOOL)defaultNonModalPromoIsShowing {
  return self.nonModalPromoCoordinator != nil;
}

- (void)dismissDefaultNonModalPromoAnimated:(BOOL)animated
                                 completion:(void (^)())completion {
  [self.nonModalPromoCoordinator dismissInfobarBannerAnimated:animated
                                                   completion:completion];
}

#pragma mark - EnterprisePromptCoordinatorDelegate

- (void)hideEnterprisePrompForLearnMore:(BOOL)learnMore {
  [self.enterprisePromptCoordinator stop];
  self.enterprisePromptCoordinator = nil;
}

#pragma mark - NetExportTabHelperDelegate

- (void)netExportTabHelper:(NetExportTabHelper*)tabHelper
    showMailComposerWithContext:(ShowMailComposerContext*)context {
  self.netExportCoordinator = [[NetExportCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
             mailComposerContext:context];

  [self.netExportCoordinator start];
}

#pragma mark - PasswordSuggestionCoordinatorDelegate

- (void)closePasswordSuggestion {
  [self.passwordSuggestionCoordinator stop];
  self.passwordSuggestionCoordinator = nil;
}

#pragma mark - PreloadControllerDelegate methods

- (web::WebState*)webStateToReplace {
  return self.browser ? self.browser->GetWebStateList()->GetActiveWebState()
                      : nullptr;
}

- (UIView*)webViewContainer {
  return self.browserContainerCoordinator.viewController.view;
}

#pragma mark - SyncPresenter (Public)

- (void)showReauthenticateSignin {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
              showSignin:
                  [[ShowSigninCommand alloc]
                      initWithOperation:AuthenticationOperationReauthenticate
                            accessPoint:signin_metrics::AccessPoint::
                                            ACCESS_POINT_UNKNOWN]
      baseViewController:self.viewController];
}

- (void)showSyncPassphraseSettings {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showSyncPassphraseSettingsFromViewController:self.viewController];
}

- (void)showGoogleServicesSettings {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showGoogleServicesSettingsFromViewController:self.viewController];
}

- (void)showAccountSettings {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showAccountsSettingsFromViewController:self.viewController];
}

- (void)showTrustedVaultReauthForFetchKeysWithTrigger:
    (syncer::TrustedVaultUserActionTriggerForUMA)trigger {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showTrustedVaultReauthForFetchKeysFromViewController:self.viewController
                                                   trigger:trigger];
}

- (void)showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:
    (syncer::TrustedVaultUserActionTriggerForUMA)trigger {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
      showTrustedVaultReauthForDegradedRecoverabilityFromViewController:
          self.viewController
                                                                trigger:
                                                                    trigger];
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  [HandlerForProtocol(self.dispatcher, ApplicationCommands)
              showSignin:command
      baseViewController:self.viewController];
}

#pragma mark - SnapshotGeneratorDelegate methods
// TODO(crbug.com/1272491): Refactor snapshot generation into (probably) a
// mediator with a narrowly-defined API to get UI-layer information from the
// BVC.

- (BOOL)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
    canTakeSnapshotForWebState:(web::WebState*)webState {
  DCHECK(webState);
  PagePlaceholderTabHelper* pagePlaceholderTabHelper =
      PagePlaceholderTabHelper::FromWebState(webState);
  return !pagePlaceholderTabHelper->displaying_placeholder() &&
         !pagePlaceholderTabHelper->will_add_placeholder_for_next_navigation();
}

- (UIEdgeInsets)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
    snapshotEdgeInsetsForWebState:(web::WebState*)webState {
  DCHECK(webState);

  UIEdgeInsets maxViewportInsets =
      _fullscreenController->GetMaxViewportInsets();

  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  if (NTPHelper && NTPHelper->IsActive()) {
    BOOL canShowTabStrip = IsRegularXRegularSizeClass(self.viewController);
    // If the NTP is active, then it's used as the base view for snapshotting.
    // When the tab strip is visible, or for the incognito NTP, the NTP is laid
    // out between the toolbars, so it should not be inset while snapshotting.
    if (canShowTabStrip || self.browser->GetBrowserState()->IsOffTheRecord()) {
      return UIEdgeInsetsZero;
    }

    // For the regular NTP without tab strip, it sits above the bottom toolbar
    // but, since it is displayed as full-screen at the top, it requires maximum
    // viewport insets.
    maxViewportInsets.bottom = 0;
    return maxViewportInsets;
  } else {
    // If the NTP is inactive, the WebState's view is used as the base view for
    // snapshotting.  If fullscreen is implemented by resizing the scroll view,
    // then the WebState view is already laid out within the visible viewport
    // and doesn't need to be inset.  If fullscreen uses the content inset, then
    // the WebState view is laid out fullscreen and should be inset by the
    // viewport insets.
    return _fullscreenController->ResizesScrollView() ? UIEdgeInsetsZero
                                                      : maxViewportInsets;
  }
}

- (NSArray<UIView*>*)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
           snapshotOverlaysForWebState:(web::WebState*)webState {
  DCHECK(webState);
  WebStateList* webStateList = self.browser->GetWebStateList();
  DCHECK_NE(webStateList->GetIndexOfWebState(webState),
            WebStateList::kInvalidIndex);
  BOOL isWebUsageEnabled =
      self.browser->GetBrowserState() && self.started &&
      WebUsageEnablerBrowserAgent::FromBrowser(self.browser)
          ->IsWebUsageEnabled();

  if (!isWebUsageEnabled || webState != webStateList->GetActiveWebState())
    return @[];

  NSMutableArray<UIView*>* overlays = [NSMutableArray array];

  UIView* downloadManagerView = _downloadManagerCoordinator.viewController.view;
  if (downloadManagerView) {
    [overlays addObject:downloadManagerView];
  }

  UIView* sadTabView = self.sadTabCoordinator.viewController.view;
  if (sadTabView) {
    [overlays addObject:sadTabView];
  }

  BrowserContainerViewController* browserContainerViewController =
      self.browserContainerCoordinator.viewController;
  // The overlay container view controller is presenting something if it has
  // a `presentedViewController` AND that view controller's
  // `presentingViewController` is the overlay container. Otherwise, some other
  // view controller higher up in the hierarchy is doing the presenting. E.g.
  // for the overflow menu, the BVC (and eventually the tab grid view
  // controller) are presenting the overflow menu, but because those view
  // controllers are also above tthe `overlayContainerViewController` in the
  // view hierarchy, the overflow menu view controller is also the
  // `overlayContainerViewController`'s presentedViewController.
  UIViewController* overlayContainerViewController =
      browserContainerViewController.webContentsOverlayContainerViewController;
  UIViewController* presentedOverlayViewController =
      overlayContainerViewController.presentedViewController;
  if (presentedOverlayViewController &&
      presentedOverlayViewController.presentingViewController ==
          overlayContainerViewController) {
    [overlays addObject:presentedOverlayViewController.view];
  }

  UIView* screenTimeView =
      browserContainerViewController.screenTimeViewController.view;
  if (screenTimeView) {
    [overlays addObject:screenTimeView];
  }

  UIView* childOverlayView =
      overlayContainerViewController.childViewControllers.firstObject.view;
  if (childOverlayView) {
    DCHECK_EQ(1U, overlayContainerViewController.childViewControllers.count);
    [overlays addObject:childOverlayView];
  }

  return overlays;
}

- (void)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
    willUpdateSnapshotForWebState:(web::WebState*)webState {
  DCHECK(webState);

  if (self.isNTPActiveForCurrentWebState) {
    [_ntpCoordinator willUpdateSnapshot];
  }
  OverscrollActionsTabHelper::FromWebState(webState)->Clear();
}

- (UIView*)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
         baseViewForWebState:(web::WebState*)webState {
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  if (NTPHelper && NTPHelper->IsActive())
    return _ntpCoordinator.viewController.view;
  return webState->GetView();
}

- (UIViewTintAdjustmentMode)snapshotGenerator:
                                (SnapshotGenerator*)snapshotGenerator
         defaultTintAdjustmentModeForWebState:(web::WebState*)webState {
  return UIViewTintAdjustmentModeAutomatic;
}

#pragma mark - NewTabPageCommands

- (void)openNTPScrolledIntoFeedType:(FeedType)feedType {
  // Dismiss any presenting modal. Ex. Follow management page.

  __weak __typeof(self) weakSelf = self;
  [self.viewController
      clearPresentedStateWithCompletion:^{
        [weakSelf scrollToNTPAfterPresentedStateCleared:feedType];
      }
                         dismissOmnibox:YES];
}

- (void)updateFollowingFeedHasUnseenContent:(BOOL)hasUnseenContent {
  [_ntpCoordinator updateFollowingFeedHasUnseenContent:hasUnseenContent];
}

- (void)handleFeedModelDidEndUpdates:(FeedType)feedType {
  [_ntpCoordinator handleFeedModelDidEndUpdates:feedType];
}

- (void)scrollToNTPAfterPresentedStateCleared:(FeedType)feedType {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();

  // Configure next NTP to be scrolled into `feedType`.
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(currentWebState);
  if (NTPHelper) {
    NTPHelper->SetNextNTPFeedType(feedType);
    // TODO(crbug.com/1329173): Scroll into feed.
  }

  // Navigate to NTP in same tab.
  UrlLoadingBrowserAgent* urlLoadingBrowserAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  UrlLoadParams urlLoadParams =
      UrlLoadParams::InCurrentTab(GURL(kChromeUINewTabURL));
  urlLoadingBrowserAgent->Load(urlLoadParams);
}

#pragma mark - WebNavigationNTPDelegate

- (BOOL)isNTPActiveForCurrentWebState {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (currentWebState) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(currentWebState);
    return NTPHelper && NTPHelper->IsActive();
  }
  return NO;
}

- (void)reloadNTPForWebState:(web::WebState*)webState {
  [_ntpCoordinator reload];
}

#pragma mark - PageInfoPresentation

- (void)presentPageInfoView:(UIView*)pageInfoView {
  [pageInfoView setFrame:self.viewController.view.bounds];
  [self.viewController.view addSubview:pageInfoView];
}

- (void)prepareForPageInfoPresentation {
  // Dismiss the omnibox (if open).
  id<OmniboxCommands> omniboxHandler =
      HandlerForProtocol(_dispatcher, OmniboxCommands);
  [omniboxHandler cancelOmniboxEdit];
}

- (CGPoint)convertToPresentationCoordinatesForOrigin:(CGPoint)origin {
  return [self.viewController.view convertPoint:origin fromView:nil];
}

#pragma mark - PasswordSettingsCoordinatorDelegate

- (void)passwordSettingsCoordinatorDidRemove:
    (PasswordSettingsCoordinator*)coordinator {
  DCHECK_EQ(self.passwordSettingsCoordinator, coordinator);
  [self.passwordSettingsCoordinator stop];
  self.passwordSettingsCoordinator.delegate = nil;
  self.passwordSettingsCoordinator = nil;
}

@end
