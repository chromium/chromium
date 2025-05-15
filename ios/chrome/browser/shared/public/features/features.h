// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_FEATURES_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_FEATURES_H_

#import <Foundation/Foundation.h>

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/notifications_promo_view_constants.h"

enum class FeedActivityBucket;

namespace base {
class TimeDelta;
}  // namespace base

// Feature flag to enable personalized messaging for Default Browser First Run,
// Set Up List, and video promos.
BASE_DECLARE_FEATURE(kSegmentedDefaultBrowserPromo);

// Name of the parameter that controls the experiment type for the Segmented
// Default Browser promo, determining whether or not the Default Browser promo
// is animated.
extern const char kSegmentedDefaultBrowserExperimentType[];

// Defines the different experiment arms for the Segmented Default Browser
// promo, which determine if the Default Browser promo is animated.
enum class SegmentedDefaultBrowserExperimentType {
  // The experiment arm that shows the static Default Browser promo.
  kStaticPromo = 0,
  // The experiment arm that show the animated Default Browser promo.
  kAnimatedPromo = 1,
};

// Whether personalized messaging for Default Browser First Run, Set Up List,
// and video promos is enabled.
bool IsSegmentedDefaultBrowserPromoEnabled();

// Returns the experiment type for the Segmented Default Browser promo, which
// determines whether or not the promo is animated.
SegmentedDefaultBrowserExperimentType
SegmentedDefaultBrowserExperimentTypeEnabled();

// Feature flag to enable the Keyboard Accessory Upgrade for iPads.
BASE_DECLARE_FEATURE(kIOSKeyboardAccessoryUpgradeForIPad);

// Feature flag to enable the Keyboard Accessory Upgrade with a shorter manual
// fill menu.
BASE_DECLARE_FEATURE(kIOSKeyboardAccessoryUpgradeShortManualFillMenu);

// Test-only: Feature flag used to verify that EG2 can trigger flags. Must be
// always disabled by default, because it is used to verify that enabling
// features in tests works.
BASE_DECLARE_FEATURE(kTestFeature);

// Killswitch to control how Safety Checks are automatically triggered.
// If enabled, the Safety Check Manager can independently initiate Safety
// Checks. If disabled, automatic Safety Check runs must be triggered through
// the Safety Check module in the Magic Stack.
BASE_DECLARE_FEATURE(kSafetyCheckAutorunByManagerKillswitch);

// Feature to add the Safety Check module to the Magic Stack.
BASE_DECLARE_FEATURE(kSafetyCheckMagicStack);

// Killswitch for conditionally hiding the Safety Check module in the Magic
// Stack if no issues are found.
BASE_DECLARE_FEATURE(kSafetyCheckModuleHiddenIfNoIssuesKillswitch);

// Feature to enable Safety Check Push Notifications.
BASE_DECLARE_FEATURE(kSafetyCheckNotifications);

// A parameter defining whether to enable provisional Safety Check
// notifications. If enabled, Safety Check notifications may be shown to the
// user even if the criteria for sending a notification are not definitively
// met.
extern const char kSafetyCheckNotificationsProvisionalEnabled[];

// Returns true if provisional Safety Check notifications are enabled.
bool ProvisionalSafetyCheckNotificationsEnabled();

// A parameter defining the duration to suppress scheduling new Safety Check
// notifications if one is already present in the notification center.
extern const char kSafetyCheckNotificationsSuppressDelayIfPresent[];

// A parameter defining the duration of user inactivity required before
// displaying Safety Check push notifications.
extern const char kSafetyCheckNotificationsUserInactiveThreshold[];

// Returns the duration of time to suppress scheduling new Safety Check
// notifications if one is already present in the notification center.
const base::TimeDelta SuppressDelayForSafetyCheckNotificationsIfPresent();

// Returns the time duration of user inactivity that must elapse before Safety
// Check notifications are displayed.
const base::TimeDelta InactiveThresholdForSafetyCheckNotifications();

// A parameter representing how many hours must elapse before the Safety Check
// is automatically run in the Magic Stack.
extern const char kSafetyCheckMagicStackAutorunHoursThreshold[];

// How many hours between each autorun of the Safety Check in the Magic Stack.
const base::TimeDelta TimeDelayForSafetyCheckAutorun();

// Feature to enable the refactored implementation of the `OmahaService`, using
// new `OmahaServiceObserver`(s) for Omaha clients. Acts as a killswitch.
BASE_DECLARE_FEATURE(kOmahaServiceRefactor);

// Safety Check Notifications experiment variations.

// Name of the experiment that controls how Safety Check notifications
// are presented to the user (e.g., `kVerbose`, `kSuccinct`).
extern const char kSafetyCheckNotificationsExperimentType[];

// Name of the parameter that controls when an impression is counted
// for the Safety Check notifications opt-in button (e.g., `kOnlyWhenTopModule`,
// `kAlways`).
extern const char kSafetyCheckNotificationsImpressionTrigger[];

// Name of the parameter that controls the maximum number of impressions
// allowed for the Safety Check notifications opt-in button.
extern const char kSafetyCheckNotificationsImpressionLimit[];

// Name of the parameter that controls whether Passwords notifications
// are permitted to be sent to the user for Safety Check.
extern const char kSafetyCheckAllowPasswordsNotifications[];

// Name of the parameter that controls whether Safe Browsing notifications
// are permitted to be sent to the user for Safety Check.
extern const char kSafetyCheckAllowSafeBrowsingNotifications[];

// Name of the parameter that controls whether Update Chrome notifications
// are permitted to be sent to the user for Safety Check.
extern const char kSafetyCheckAllowUpdateChromeNotifications[];

// Defines param values for the Safety Check Notifications feature,
// controlling how notifications are presented to the user.
enum class SafetyCheckNotificationsExperimentalArm {
  // Arm that displays multiple Safety Check notifications at any given time.
  kVerbose = 0,
  // Arm that displays only a single Safety Check notification at any given
  // time.
  kSuccinct = 1,
};

// Defines param values for the Safety Check Notifications feature,
// controlling when an impression is counted for the notifications opt-in button
// in the Safety Check (Magic Stack) module.
enum class SafetyCheckNotificationsImpressionTrigger {
  // Impression counted only when the Safety Check module is the top module.
  kOnlyWhenTopModule = 0,
  // Impression counted regardless of the Safety Check module's position.
  kAlways = 1,
};

// Name of the parameter that controls the experiment type for the Lens Shop
// tip, determining whether or not a product image is displayed.
extern const char kTipsLensShopExperimentType[];

// Defines the different experiment arms for the Lens Shop tip, which
// determine whether or not a product image is displayed (if available).
enum class TipsLensShopExperimentType {
  // The experiment arm that shows the product image (if available) in the
  // Lens Shop tip.
  kWithProductImage = 0,
  // The experiment arm that does not show the product image in the Lens shop
  // tip.
  kWithoutProductImage = 1,
};

// Name of the parameter that controls the experiment type for the Enhanced Safe
// Browsing tip, determining whether to show the animated, instructional promo
// or the Safe Browsing settings page.
extern const char kTipsSafeBrowsingExperimentType[];

// Defines the different experiment arms for the Enhanced Safe Browsing tip.
enum class TipsSafeBrowsingExperimentType {
  // Shows the animated, instructional Enhanced Safe Browsing promo.
  kShowEnhancedSafeBrowsingPromo = 0,
  // Shows the Safe Browsing settings page.
  kShowSafeBrowsingSettingsPage = 1,
};

// Feature flag to enable Shared Highlighting (Link to Text).
BASE_DECLARE_FEATURE(kSharedHighlightingIOS);

// Feature flag to enable Share button in web context menu in iOS.
BASE_DECLARE_FEATURE(kShareInWebContextMenuIOS);

// Feature flag to log metrics for the edit menu.
BASE_DECLARE_FEATURE(kIOSBrowserEditMenuMetrics);

// Docking Promo experiment variations.

// A parameter representing the experimental arm for when the Docking Promo is
// displayed: during the FRE, or after the FRE.
extern const char kIOSDockingPromoExperimentType[];

// A parameter representing how many hours of inactivity are required (for users
// no older than 2 days) before the Docking Promo is shown. This parameter is
// only used if `kIOSDockingPromoNewUserInactiveThreshold` is not set.
extern const char kIOSDockingPromoNewUserInactiveThresholdHours[];

// A parameter representing how many hours of inactivity are required (for users
// no older than 14 days) before the Docking Promo is shown. This parameter is
// only used if `kIOSDockingPromoOldUserInactiveThreshold` is not set.
extern const char kIOSDockingPromoOldUserInactiveThresholdHours[];

// Minimum duration of inactivity required before showing the Docking Promo to
// new users (<= 2 days old).
extern const char kIOSDockingPromoNewUserInactiveThreshold[];

// Minimum duration of inactivity required before showing the Docking Promo to
// old users (<= 14 days old).
extern const char kIOSDockingPromoOldUserInactiveThreshold[];

// Feature flag to enable the Docking Promo.
BASE_DECLARE_FEATURE(kIOSDockingPromo);

// Feature flag to enable the Docking Promo feature exclusively for users who
// first meet the promo's eligibility criteria.
//
// NOTE: This feature flag exists to improve metrics logging to better
// understand the feature's impact on user engagement and conversion rates.
BASE_DECLARE_FEATURE(kIOSDockingPromoForEligibleUsersOnly);

// Killswitch to enable the fixed Docking Promo trigger logic.
BASE_DECLARE_FEATURE(kIOSDockingPromoFixedTriggerLogicKillswitch);

// Killswitch to prevent the Docking Promo from being deregistered in the Promos
// Manager.
BASE_DECLARE_FEATURE(kIOSDockingPromoPreventDeregistrationKillswitch);

// Param values for the Docking Promo display trigger experimental arms.
enum class DockingPromoDisplayTriggerArm {
  kAfterFRE = 0,
  kAppLaunch = 1,
  kDuringFRE = 2,
};

// Helper function to check if `kIOSDockingPromo` is enabled.
bool IsDockingPromoEnabled();

// Helper function to check if `kIOSDockingPromoForEligibleUsersOnly` is
// enabled.
bool IsDockingPromoForEligibleUsersOnlyEnabled();

// Returns the experiment type for the Docking Promo feature.
DockingPromoDisplayTriggerArm DockingPromoExperimentTypeEnabled();

// For users no older than 2 days, how many hours of inactivity must pass before
// showing the Docking Promo.
int HoursInactiveForNewUsersUntilShowingDockingPromo();

// For users no older than 14 days, how many hours of inactivity must pass
// before showing the Docking Promo.
int HoursInactiveForOldUsersUntilShowingDockingPromo();

// Minimum inactivity duration (between app launches) before showing the Docking
// Promo to new users.
const base::TimeDelta InactiveThresholdForNewUsersUntilDockingPromoShown();

// Minimum inactivity duration (between app launches) before showing the Docking
// Promo to old users.
const base::TimeDelta InactiveThresholdForOldUsersUntilDockingPromoShown();

// Feature flag to use direct upload for Lens searches.
BASE_DECLARE_FEATURE(kIOSLensUseDirectUpload);

// Feature flag to enable the Lens entrypoint in the home screen widget.
BASE_DECLARE_FEATURE(kEnableLensInHomeScreenWidget);

// Feature flag to enable the Lens entrypoint in the keyboard.
BASE_DECLARE_FEATURE(kEnableLensInKeyboard);

// Feature flag to enable the Lens entrypoint in the new tab page.
BASE_DECLARE_FEATURE(kEnableLensInNTP);

// Feature flag to enable the Lens "Search copied image" omnibox entrypoint.
BASE_DECLARE_FEATURE(kEnableLensInOmniboxCopiedImage);

// Feature flag to enable the Lens "Search copied image" omnibox entrypoint.
BASE_DECLARE_FEATURE(kEnableLensOverlay);
extern const base::NotFatalUntil kLensOverlayNotFatalUntil;

// Feature flag to enable the Lens View Finder Unified experience
BASE_DECLARE_FEATURE(kEnableLensViewFinderUnifiedExperience);

// Feature flag to enable the Lens Context Menu Unified experience
BASE_DECLARE_FEATURE(kEnableLensContextMenuUnifiedExperience);

// Whether to enable loading AIM in the lens result page.
BASE_DECLARE_FEATURE(kLensLoadAIMInLensResultPage);

// Feature flag to enable the Lens overlay location bar entrypoint. Enabled by
// default.
BASE_DECLARE_FEATURE(kLensOverlayEnableLocationBarEntrypoint);

// Feature flag to enable the Lens overlay location bar entrypoint on SRP.
// Enabled by default.
BASE_DECLARE_FEATURE(kLensOverlayEnableLocationBarEntrypointOnSRP);

// Feature flag to disable price insights for a lens overlay experiment. As the
// price insights entrypoint trumps the lens overlay entrypoint. This flag
// should only be used for experiment.
BASE_DECLARE_FEATURE(kLensOverlayDisablePriceInsights);

// Feature flag to enable lens overlay location bar entrypoint only when price
// insights should trigger. This is used as counterfactual for
// kLensOverlayDisablePriceInsights.
BASE_DECLARE_FEATURE(kLensOverlayPriceInsightsCounterfactual);

// Feature to force allow iPad support of lens overlay.
BASE_DECLARE_FEATURE(kLensOverlayEnableIPadCompatibility);

// Feature to allow landscape support of lens overlay.
BASE_DECLARE_FEATURE(kLensOverlayEnableLandscapeCompatibility);

// Feature to enable LVF escape hatch in the overflow menu in Lens overlay.
BASE_DECLARE_FEATURE(kLensOverlayEnableLVFEscapeHatch);

// Feature to open lens overlay navigation in the same tab.
BASE_DECLARE_FEATURE(kLensOverlayEnableSameTabNavigation);

// Feature to enable force showing the lens overlay onboarding screen.
BASE_DECLARE_FEATURE(kLensOverlayForceShowOnboardingScreen);

// Types of lens overlay onboarding.
extern const char kLensOverlayOnboardingParam[];
extern const char kLensOverlayOnboardingParamSpeedbumpMenu[];
extern const char kLensOverlayOnboardingParamUpdatedStrings[];
extern const char kLensOverlayOnboardingParamUpdatedStringsAndVisuals[];

// Feature flag to change the onboariding experience of Lens Overlay.
BASE_DECLARE_FEATURE(kLensOverlayAlternativeOnboarding);

// Feature flag to add lens overlay navigation to history.
BASE_DECLARE_FEATURE(kLensOverlayNavigationHistory);

// Feature flag to enable UITraitCollection workaround for fixing incorrect
// trait propagation.
BASE_DECLARE_FEATURE(kEnableTraitCollectionWorkAround);

// Feature flag to enable duplicate NTP cleanup.
BASE_DECLARE_FEATURE(kRemoveExcessNTPs);

// Feature flag / Kill Switch for TCRex.
BASE_DECLARE_FEATURE(kTCRexKillSwitch);

// When enabled uses new transitions in the TabGrid.
BASE_DECLARE_FEATURE(kTabGridNewTransitions);

// Whether the new tab grid tabs transitions should be enabled.
bool IsNewTabGridTransitionsEnabled();

// Feature to enable force showing the Contextual Panel entrypoint.
BASE_DECLARE_FEATURE(kContextualPanelForceShowEntrypoint);

bool IsContextualPanelForceShowEntrypointEnabled();

// Feature to enable the contextual panel.
BASE_DECLARE_FEATURE(kContextualPanel);

bool IsContextualPanelEnabled();

// A parameter representing how many seconds delay before the large Contextual
// Panel Entrypoint is shown (timer starts after the normal entrypoint is
// shown).
extern const base::FeatureParam<int>
    kLargeContextualPanelEntrypointDelayInSeconds;
// A parameter representing how many seconds the large Contextual Panel
// Entrypoint is shown for, which includes disabling fullscreen.
extern const base::FeatureParam<int>
    kLargeContextualPanelEntrypointDisplayedInSeconds;

int LargeContextualPanelEntrypointDelayInSeconds();
int LargeContextualPanelEntrypointDisplayedInSeconds();

// A parameter representing whether the Contextual Panel entrypoint should be
// highlighted in blue when showing an IPH.
extern const base::FeatureParam<bool>
    kContextualPanelEntrypointHighlightDuringIPH;

bool ShouldHighlightContextualPanelEntrypointDuringIPH();

// A parameter representing whether the Contextual Panel entrypoint should show
// a rich IPH.
extern const base::FeatureParam<bool> kContextualPanelEntrypointRichIPH;

bool ShouldShowRichContextualPanelEntrypointIPH();

// Feature flag to control the maximum amount of non-modal DB promo impressions
// server-side. Enabled by default to always have a default impression limit
// value.
BASE_DECLARE_FEATURE(kNonModalDefaultBrowserPromoImpressionLimit);

// The default param value for the non-modal DB promo impression limit,
// overridable through Finch. The associated histogram supports a maximum of 10
// impressions.
extern const base::FeatureParam<int>
    kNonModalDefaultBrowserPromoImpressionLimitParam;

// Flag to enable push notification settings menu item.
BASE_DECLARE_FEATURE(kNotificationSettingsMenuItem);

// Feature param under kBottomOmniboxDefaultSetting to select the default
// setting.
extern const char kBottomOmniboxDefaultSettingParam[];
extern const char kBottomOmniboxDefaultSettingParamTop[];
extern const char kBottomOmniboxDefaultSettingParamBottom[];
extern const char kBottomOmniboxDefaultSettingParamSafariSwitcher[];
// Feature flag to change the default position of the omnibox.
BASE_DECLARE_FEATURE(kBottomOmniboxDefaultSetting);

// Feature flag to put all clipboard access onto a background thread. Any
// synchronous clipboard access will always return nil/false.
BASE_DECLARE_FEATURE(kOnlyAccessClipboardAsync);

// Feature flag to try using the page theme color in the top toolbar
BASE_DECLARE_FEATURE(kThemeColorInTopToolbar);

// Whether the Safety Check Manager can automatically trigger Safety Checks.
bool IsSafetyCheckAutorunByManagerEnabled();

// Whether the Safety Check module should be shown in the Magic Stack.
bool IsSafetyCheckMagicStackEnabled();

// Whether the Safety Check module is hidden when no issues are found.
bool ShouldHideSafetyCheckModuleIfNoIssues();

// Whether Safety Check Push Notifications should be sent to the user.
bool IsSafetyCheckNotificationsEnabled();

// Checks if Passwords notifications are permitted to be sent to the user
// for Safety Check, based on the Finch parameter
// `kSafetyCheckAllowPasswordsNotifications`.
bool AreSafetyCheckPasswordsNotificationsAllowed();

// Checks if Safe Browsing notifications are permitted to be sent to the user
// for Safety Check, based on the Finch parameter
// `kSafetyCheckAllowSafeBrowsingNotifications`.
bool AreSafetyCheckSafeBrowsingNotificationsAllowed();

// Checks if Update Chrome notifications are permitted to be sent to the user
// for Safety Check, based on the Finch parameter
// `kSafetyCheckAllowUpdateChromeNotifications`.
bool AreSafetyCheckUpdateChromeNotificationsAllowed();

// Whether the Tips module should be shown in the Magic Stack.
bool IsTipsMagicStackEnabled();

// Returns the experiment type for the Lens Shop tip, which determines
// whether or not a product image is displayed (if available).
TipsLensShopExperimentType TipsLensShopExperimentTypeEnabled();

// Returns the experiment type for the Enhanced Safe Browsing tip, which
// determines whether to show the animated, instructional promo or the Safe
// Browsing settings page.
TipsSafeBrowsingExperimentType TipsSafeBrowsingExperimentTypeEnabled();

// Whether the refactored implementation of the `OmahaService` is enabled.
bool IsOmahaServiceRefactorEnabled();

// Returns the experiment type for the Safety Check Notifications feature.
SafetyCheckNotificationsExperimentalArm
SafetyCheckNotificationsExperimentTypeEnabled();

// Returns the impression trigger for the Safety Check (Magic Stack) module's
// notification opt-in button.
SafetyCheckNotificationsImpressionTrigger
SafetyCheckNotificationsImpressionTriggerEnabled();

// Returns the maximum number of impressions allowed for the Safety Check
// notifications opt-in button, as specified by the
// `kSafetyCheckNotificationsImpressionLimit` field trial parameter.
int SafetyCheckNotificationsImpressionLimit();

// Feature flag enabling Choose from Drive.
BASE_DECLARE_FEATURE(kIOSChooseFromDrive);
// Feature flag enabling support for simulated clicks in Choose from Drive.
BASE_DECLARE_FEATURE(kIOSChooseFromDriveSimulatedClick);

// Feature flag enabling a fix for the Download manager mediator.
BASE_DECLARE_FEATURE(kIOSDownloadNoUIUpdateInBackground);

// Feature flag enabling account storage management.
BASE_DECLARE_FEATURE(kIOSManageAccountStorage);

// Feature flag to enable feed background refresh.
// Use IsFeedBackgroundRefreshEnabled() instead of this constant directly.
BASE_DECLARE_FEATURE(kEnableFeedBackgroundRefresh);

// Feature flag to deprecate the "Discover / Follow" toggle from the header of
// the feed. When this feature is enabled, there would not be a separate
// following feed.
BASE_DECLARE_FEATURE(kDeprecateFeedHeader);
bool ShouldDeprecateFeedHeader();

// Feature flag to disable the feed.
BASE_DECLARE_FEATURE(kEnableFeedAblation);

// Content Push Notifications Variations.
extern const char kContentPushNotificationsExperimentType[];

// Feature flag to enable the content notifications.
BASE_DECLARE_FEATURE(kContentPushNotifications);

// Feature flag to enable Content Notification experiments.
BASE_DECLARE_FEATURE(kContentNotificationExperiment);

// Feature flag to enable Content Notification Provisional without any
// conditions.
BASE_DECLARE_FEATURE(kContentNotificationProvisionalIgnoreConditions);

// True if Content Notification Provisional is enabled without any conditions.
bool IsContentNotificationProvisionalIgnoreConditions();

// Flag to override delivered NAUs.
BASE_DECLARE_FEATURE(kContentNotificationDeliveredNAU);

// Parameter value for the max number of delivered NAUs to be sent per session.
extern const char kDeliveredNAUMaxPerSession[];

// Feature flag to enable a more stable fullscreen.
BASE_DECLARE_FEATURE(kFullscreenImprovement);

// Whether the Tab Groups should be enabled in the Grid.
bool IsTabGroupInGridEnabled();

// Feature flag to enable Tab Group Sync.
BASE_DECLARE_FEATURE(kTabGroupSync);

// Whether the tab groups should be syncing.
bool IsTabGroupSyncEnabled();

// Feature flag to enable Tab Group Indicator.
BASE_DECLARE_FEATURE(kTabGroupIndicator);

// Whether the Tab Group Indicator feature is enabled.
bool IsTabGroupIndicatorEnabled();

// Whether the TabGroup send feedback button is enabled.
// TODO(crbug.com/398183785): Remove once we got feedback.
bool IsTabGroupSendFeedbackAvailable();

// Feature flag to enable a new illustration in the sync opt-in promotion view.
BASE_DECLARE_FEATURE(kNewSyncOptInIllustration);

// Whether the kNewSyncOptInIllustration feature is enabled.
bool IsNewSyncOptInIllustration();

// Feature flag to disable Lens LVF features.
BASE_DECLARE_FEATURE(kDisableLensCamera);

// YES when the Downloads Auto Deletion feature is enabled.
BASE_DECLARE_FEATURE(kDownloadAutoDeletionFeatureEnabled);

// Whether the kDownloadAutoDeletion feature is enabled.
bool IsDownloadAutoDeletionFeatureEnabled();

// Feature flag that allows opening the downloaded PDF files in Chrome.
BASE_DECLARE_FEATURE(kDownloadedPDFOpening);

// Feature param under `kEnableFeedBackgroundRefresh` to also enable background
// refresh for the Following feed.
extern const char kEnableFollowingFeedBackgroundRefresh[];

// Feature param under `kEnableFeedBackgroundRefresh` to enable server driven
// background refresh schedule.
extern const char kEnableServerDrivenBackgroundRefreshSchedule[];

// Feature param under `kEnableFeedBackgroundRefresh` to enable recurring
// background refresh schedule.
extern const char kEnableRecurringBackgroundRefreshSchedule[];

// Feature param under `kEnableFeedBackgroundRefresh` for the max age that the
// cache is still considered fresh.
extern const char kMaxCacheAgeInSeconds[];

// Feature param under `kEnableFeedBackgroundRefresh` for the background refresh
// interval in seconds.
extern const char kBackgroundRefreshIntervalInSeconds[];

// Feature param under `kEnableFeedBackgroundRefresh` for the background refresh
// max age in seconds. This value is compared against the age of the feed when
// performing a background refresh. A zero value means the age check is ignored.
extern const char kBackgroundRefreshMaxAgeInSeconds[];

// Whether the Following Feed is enabled on NTP.
bool IsWebChannelsEnabled();

// Whether the Discover service is created early, alongside the app creation.
bool IsDiscoverFeedServiceCreatedEarly();

// Whether feed background refresh is enabled.
bool IsFeedBackgroundRefreshEnabled();

// Saves whether any background refresh experiment is enabled. This call
// DCHECKs on the availability of `base::FeatureList`.
void SaveFeedBackgroundRefreshCapabilityEnabledForNextColdStart();

// Sets `timestamp` for key `NSUserDefaultsKey` to be displayed in Experimental
// Settings in the Settings App. This is not available in stable.
void SetFeedRefreshTimestamp(NSDate* timestamp, NSString* NSUserDefaultsKey);

// Returns the override value from Experimental Settings in the Settings App. If
// enabled, all values in Experimental Settings will override all corresponding
// defaults.
bool IsFeedOverrideDefaultsEnabled();

// Whether the Following feed should also be refreshed in the background.
bool IsFollowingFeedBackgroundRefreshEnabled();

// Whether the background refresh schedule should be driven by server values.
bool IsServerDrivenBackgroundRefreshScheduleEnabled();

// Whether a new refresh should be scheduled after completion of a previous
// background refresh. Not currently used in code.
bool IsRecurringBackgroundRefreshScheduleEnabled();

// Returns the max age that the cache is still considered fresh. In other words,
// the feed freshness threshold.
double GetFeedMaxCacheAgeInSeconds();

// The earliest interval to refresh if server value is not used. This value is
// an input into the DiscoverFeedService.
double GetBackgroundRefreshIntervalInSeconds();

// Returns the background refresh max age in seconds.
double GetBackgroundRefreshMaxAgeInSeconds();

// Whether the feed is disabled.
bool IsFeedAblationEnabled();

// YES when Follow UI Update is enabled.
bool IsFollowUIUpdateEnabled();

// YES if content push notification experiments are enabled.
bool IsContentNotificationExperimentEnabled();

// YES when any of the content push notification variations are enabled.
bool IsContentPushNotificationsEnabled();

// Returns the Experiment type from the content push notifications flag.
NotificationsExperimentType ContentNotificationsExperimentTypeEnabled();

// YES when the Content Push Notifications Promo is enabled.
bool IsContentPushNotificationsPromoEnabled();

// YES when the Content Push Notifications Setup List is enabled.
bool IsContentPushNotificationsSetUpListEnabled();

// YES when the Content Provisional Push Notifications are enabled.
bool IsContentPushNotificationsProvisionalEnabled();

// YES when the Content Push Notifications Promo is registered with no UI
// change.
bool IsContentPushNotificationsPromoRegistrationOnly();

// YES when the Content Push Notifications Provisional is registered with no UI
// change.
bool IsContentPushNotificationsProvisionalRegistrationOnly();

// YES when the Content Push Notifications Set Up List is registered with no UI
// change.
bool IsContentPushNotificationsSetUpListRegistrationOnly();

// Whether or not the Keyboard Accessory Upgrade feature is enabled.
bool IsKeyboardAccessoryUpgradeEnabled();

// Whether or not the kIOSKeyboardAccessoryUpgradeShortManualFillMenu feature is
// enabled.
bool IsKeyboardAccessoryUpgradeWithShortManualFillMenuEnabled();

// Feature for the Magic Stack.
BASE_DECLARE_FEATURE(kMagicStack);

// Feature that enables tab resumption.
BASE_DECLARE_FEATURE(kTabResumption);

// Whether the tab resumption feature is enabled.
bool IsTabResumptionEnabled();

// Feature that enables images for Tab Resumption.
BASE_DECLARE_FEATURE(kTabResumptionImages);

// A parameter to choose what type of images are enabled in
// `kTabResumptionImages` experiment (default to all).
extern const char kTabResumptionImagesTypes[];

// A parameter value for `kTabResumptionImagesTypes` to only enable salient
// images images for tab resumption.
extern const char kTabResumptionImagesTypesSalient[];

// A parameter value for `kTabResumptionImagesTypes` to only enable thumbnails
// images images for tab resumption.
extern const char kTabResumptionImagesTypesThumbnails[];

// A parameter to indicate whether the native UI is enabled for the discover
// feed.
// TODO(crbug.com/40246814): Remove this.
extern const char kDiscoverFeedIsNativeUIEnabled[];

// Feature parameters for the tab resumption feature. The threshold for tabs
// fetched from sync in seconds. Default to 12 hours.
extern const char kTabResumptionThresholdParameterName[];

// Whether the tab resumption with salient images for distant tabs (or fallback
// for local tabs) is enabled.
bool IsTabResumptionImagesSalientEnabled();

// Whether the tab resumption with salient images for local tabs is enabled.
bool IsTabResumptionImagesThumbnailsEnabled();

// Convenience method for determining the tab resumption time threshold for
// X-Devices tabs only.
const base::TimeDelta TabResumptionForXDevicesTimeThreshold();

// Kill switch for disabling the navigations when the application is in
// foreground inactive state after opening an external app.
BASE_DECLARE_FEATURE(kInactiveNavigationAfterAppLaunchKillSwitch);

// Convenience method for determining if Pinned Tabs is enabled.
// The Pinned Tabs feature is fully enabled on iPhone and disabled on iPad.
bool IsPinnedTabsEnabled();

// Feature flag for caching the ios module ranker.
BASE_DECLARE_FEATURE(kSegmentationPlatformIosModuleRankerCaching);

// Whether the Segmentation Tips Manager is enabled for Chrome iOS.
bool IsSegmentationTipsManagerEnabled();

// Flag to not keep a strong reference to the spotlight index, as a tentative
// memory improvement measure.
BASE_DECLARE_FEATURE(kSpotlightNeverRetainIndex);

// Feature that enables improvements for Save to Photos feature.
BASE_DECLARE_FEATURE(kIOSSaveToPhotosImprovements);

// A set of parameters to indicate which improvement to apply to the Save to
// Photos feature.
extern const char kSaveToPhotosContextMenuImprovementParam[];
extern const char kSaveToPhotosTitleImprovementParam[];
extern const char kSaveToPhotosAccountDefaultChoiceImprovementParam[];

// Returns true if the Save to Photos action improvement is enabled.
bool IsSaveToPhotosActionImprovementEnabled();

// Returns true if the Save to Photos title improvement is enabled.
bool IsSaveToPhotosTitleImprovementEnabled();

// Returns true if the Save to Photos account picker improvement is enabled.
bool IsSaveToPhotosAccountPickerImprovementEnabled();

// Feature flag to enable app background refresh.
// Use IsAppBackgroundRefreshEnabled() instead of this constant directly.
BASE_DECLARE_FEATURE(kEnableAppBackgroundRefresh);

// Whether app background refresh is enabled.
bool IsAppBackgroundRefreshEnabled();

// Feature flag for changes that aim to improve memory footprint on the Home
// surface.
BASE_DECLARE_FEATURE(kHomeMemoryImprovements);

// Whether Home memory improvements are enabled.
bool IsHomeMemoryImprovementsEnabled();

// Feature flag to enable account confirmation snackbar on startup.
BASE_DECLARE_FEATURE(kIdentityConfirmationSnackbar);

// Feature params to specify how much time between identity confirmation
// snackbar triggers to avoid over-prompting. Overridable through Finch.
extern const base::FeatureParam<base::TimeDelta>
    kIdentityConfirmationMinDisplayInterval1;
extern const base::FeatureParam<base::TimeDelta>
    kIdentityConfirmationMinDisplayInterval2;
extern const base::FeatureParam<base::TimeDelta>
    kIdentityConfirmationMinDisplayInterval3;

// Feature flag to enable the registration of customized UITrait arrays. This
// feature flag is related to the effort to remove invocations of
// 'traitCollectionDidChange' which was deprecated in iOS 17.
BASE_DECLARE_FEATURE(kEnableTraitCollectionRegistration);

// Feature flag to enable displaying blue dot on tools menu button on toolbar.
BASE_DECLARE_FEATURE(kBlueDotOnToolsMenuButton);

// Returns whether `kBlueDotOnToolsMenuButton` is enabled.
bool IsBlueDotOnToolsMenuButtoneEnabled();

// Feature flag to assign each managed account to its own separate profile.
// DO NOT CHECK DIRECTLY, use AreSeparateProfilesForManagedAccountsEnabled()!
BASE_DECLARE_FEATURE(kSeparateProfilesForManagedAccounts);

// Kill switch to turn off `kSeparateProfilesForManagedAccounts`, even if
// multiple profiles already exist.
// DO NOT CHECK DIRECTLY, use AreSeparateProfilesForManagedAccountsEnabled()!
BASE_DECLARE_FEATURE(kSeparateProfilesForManagedAccountsKillSwitch);

// Feature flag to have widgets per account.
// DO NOT CHECK DIRECTLY, use IsWidgetsForMultiprofileEnabled().
BASE_DECLARE_FEATURE(kWidgetsForMultiprofile);

// Feature to control resyncing the omaha ping timer on foregrounding.
BASE_DECLARE_FEATURE(kOmahaResyncTimerOnForeground);

// Feature flag to use the async version of the chrome startup method.
BASE_DECLARE_FEATURE(kChromeStartupParametersAsync);

// Feature flag to enable the opening of links from Youtube Incognito in Chrome
// incognito.
BASE_DECLARE_FEATURE(kYoutubeIncognito);

// Feature param to specify whether the youtube incognito handling is done
// without the incognito interstitial.
extern const char
    kYoutubeIncognitoErrorHandlingWithoutIncognitoInterstitialParam[];

// A parameter to choose what type of apps allowed for `kYoutubeIncognito`
// experiment (default to allow listed)
extern const char kYoutubeIncognitoTargetApps[];

// A parameter value for `kYoutubeIncognitoTargetApps` to only enable the
// feature for the allow listed apps.
extern const char kYoutubeIncognitoTargetAppsAllowlisted[];

// A parameter value for `kYoutubeIncognitoTargetApps` to only enable the
// feature for the first party apps.
extern const char kYoutubeIncognitoTargetAppsFirstParty[];

// A parameter value for `kYoutubeIncognitoTargetApps` to only enable the
// feature for all apps.
extern const char kYoutubeIncognitoTargetAppsAll[];

// Returns whether
// `kYoutubeIncognitoErrorHandlingWithoutIncognitoInterstitialParam` is enabled.
bool IsYoutubeIncognitoErrorHandlingWithoutIncognitoInterstitialEnabled();

// Returns whether `kYoutubeIncognitoTargetApps` is
// `kYoutubeIncognitoTargetAppsAllowlisted`.
bool IsYoutubeIncognitoTargetAllowListedEnabled();

// Returns whether `kYoutubeIncognitoTargetApps` is
// `kYoutubeIncognitoTargetAppsFirstParty`.
bool IsYoutubeIncognitoTargetFirstPartyEnabled();

// Returns whether `kYoutubeIncognitoTargetApps` is
// `kYoutubeIncognitoTargetAppsAll`.
bool IsYoutubeIncognitoTargetAllEnabled();

// Feature flag to enable Reactivation Notifications.
BASE_DECLARE_FEATURE(kIOSReactivationNotifications);

// Feature param to specify how much time should elapse before a Reactivation
// notification should trigger.
extern const char kIOSReactivationNotificationsTriggerTimeParam[];

// Feature param containing a comma separated list of integers that represent
// cases of the `TipsNotificationType` enum.
extern const char kIOSReactivationNotificationsOrderParam[];

// Returns whether `kIOSReactivationNotifications` is enabled.
bool IsIOSReactivationNotificationsEnabled();

// Feature flag to enable Expanded Tips.
BASE_DECLARE_FEATURE(kIOSExpandedTips);

// Feature param containing a comma separated list of integers that represent
// cases of the `TipsNotificationType` enum.
extern const char kIOSExpandedTipsOrderParam[];

// Returns whether `kIOSExpandTips` is enabled.
bool IsIOSExpandedTipsEnabled();

// Feature flag to show an alert to the user when only provisiona notifications
// are allowed.
BASE_DECLARE_FEATURE(kProvisionalNotificationAlert);

// Returns whether `kIOSReactivationNotifications` is enabled.
bool IsProvisionalNotificationAlertEnabled();

// Feature flag to control whether the Default Browser banner promo is enabled.
BASE_DECLARE_FEATURE(kDefaultBrowserBannerPromo);

// Parameter for the number of impressions to show the promo for.
extern const base::FeatureParam<int> kDefaultBrowserBannerPromoImpressionLimit;

// Returns whether `kDefaultBrowserBannerPromo` is enabled.
bool IsDefaultBrowserBannerPromoEnabled();

// Feature to enable different text for the secondary action on FRE sign-in
// promo.
BASE_DECLARE_FEATURE(kFRESignInSecondaryActionLabelUpdate);
extern const base::FeatureParam<std::string>
    kFRESignInSecondaryActionLabelUpdateParam;
extern const std::string_view
    kFRESignInSecondaryActionLabelUpdateParamStaySignedOut;

// Returns whether 'kFRESignInSecondaryActionLabelUpdate' is enabled.
bool FRESignInSecondaryActionLabelUpdate();

// Enables passkey syncing follow-up features.
BASE_DECLARE_FEATURE(kIOSPasskeysM2);

// Helper function returning the status of `kIOSPasskeysM2`.
bool IOSPasskeysM2Enabled();

// Enables Profile-specific push notification handling logic. When enabled, this
// routes incoming notifications to the PushNotificationClientManager associated
// with the current Profile, rather than using a single global manager. This
// flag is disabled by default while the refactor is ongoing.
//
// DO NOT CHECK DIRECTLY, use IsMultiProfilePushNotificationHandlingEnabled()!
BASE_DECLARE_FEATURE(kIOSPushNotificationMultiProfile);

extern const char kFullscreenTransitionSlower[];
extern const char kFullscreenTransitionDefaultSpeed[];
extern const char kFullscreenTransitionFaster[];
extern const char kFullscreenTransitionSpeedParam[];

enum class FullscreenTransitionSpeed {
  kSlower = 0,
  kFaster = 1,
};

FullscreenTransitionSpeed FullscreenTransitionSpeedParam();

bool IsFullscreenTransitionSet();

bool IsFullscreenTransitionOffsetSet();

extern const char kMediumFullscreenTransitionOffsetParam[];

// Feature flag to changes the distance of unique scrolling before triggering
// the fullscreen transition or the speed of the transition.
BASE_DECLARE_FEATURE(kFullscreenTransition);

// Feature flag for switching the toolbar UI to an observer-based architecture.
BASE_DECLARE_FEATURE(kRefactorToolbarsSize);

bool IsRefactorToolbarsSize();

// Feature flag to enable the new share extension UI and entries.
BASE_DECLARE_FEATURE(kNewShareExtension);

// Feature that disables all IPH messages.
BASE_DECLARE_FEATURE(kIPHAblation);

// Feature that disables IPH dismissal pan gesture for lens overlay promos.
BASE_DECLARE_FEATURE(kLensOverlayDisableIPHPanGesture);

// Returns true if IPH ablation is enabled.
bool IsIPHAblationEnabled();

// Feature that prevents certain gesture recognition for IPHs.
BASE_DECLARE_FEATURE(kIPHGestureRecognitionAblation);

// Returns true if taps inside the IPH bubble should be ignored.
bool IsIPHGestureRecognitionInsideTapAblationEnabled();

// Returns true if taps outside the IPH bubble should be ignored.
bool IsIPHGestureRecognitionOutsideTapAblationEnabled();

// Returns true if pans outside the IPH bubble should be ignored.
bool IsIPHGestureRecognitionPanAblationEnabled();

// Returns true if swipes during an IPH presentation should be ignored.
bool IsIPHGestureRecognitionSwipeAblationEnabled();

// Returns true if IPH gesture recognizers should set the `cancelsTouchesInView`
// property to YES.
bool ShouldCancelTouchesInViewForIPH();

// Returns true if the IPH gesture recognition improvements are enabled.
bool IsIPHGestureRecognitionImprovementEnabled();

// Feature flag for enabling the non-modal sign-in promo.
BASE_DECLARE_FEATURE(kNonModalSignInPromo);

// Returns whether the non-modal sign-in promo is enabled.
bool IsNonModalSignInPromoEnabled();

// Feature flag to remove section breaks when detecting addresses.
BASE_DECLARE_FEATURE(kIOSOneTapMiniMapRemoveSectionsBreaks);

// Feature flags for enhanced One Tap Minimap experiment
// The main feature that controls of these restrictions. Different parameters
// control the different available restrictions.
BASE_DECLARE_FEATURE(kIOSOneTapMiniMapRestrictions);
// A parameter that requires revalidating the address using NSDataDetector.
extern const char kIOSOneTapMiniMapRestrictionCrossValidateParamName[];
extern const base::FeatureParam<bool>
    kIOSOneTapMiniMapRestrictionCrossValidateParam;
// A parameter that requires a higher confidence.
extern const char kIOSOneTapMiniMapRestrictionThreshholdParamName[];
extern const base::FeatureParam<double>
    kIOSOneTapMiniMapRestrictionThreshholdParam;
// A parameter that requires a minimum length for the address.
extern const char kIOSOneTapMiniMapRestrictionMinCharsParamName[];
extern const base::FeatureParam<int> kIOSOneTapMiniMapRestrictionMinCharsParam;
// A parameter that requires a maximum number of sections for the address.
extern const char kIOSOneTapMiniMapRestrictionMaxSectionsParamName[];
extern const base::FeatureParam<int>
    kIOSOneTapMiniMapRestrictionMaxSectionsParam;
// A parameter that the address contains a word (separated by spaces) of at
// least that number of characters.
extern const char kIOSOneTapMiniMapRestrictionLongestWordMinCharsParamName[];
extern const base::FeatureParam<int>
    kIOSOneTapMiniMapRestrictionLongestWordMinCharsParam;
// A parameter that requires having a higher proportion of alphanumerical
// characters.
extern const char kIOSOneTapMiniMapRestrictionMinAlphanumProportionParamName[];
extern const base::FeatureParam<double>
    kIOSOneTapMiniMapRestrictionMinAlphanumProportionParam;

// Returns whether notification collision management is enabled.
bool IsNotificationCollisionManagementEnabled();

// Feature flag for enabling notification collision management.
BASE_DECLARE_FEATURE(kNotificationCollisionManagement);

// Feature flag to enable integration with iOS's
// providesAppNotificationSettings.
BASE_DECLARE_FEATURE(kIOSProvidesAppNotificationSettings);

// Feature flag for enabling the sign-in button without avatar.
BASE_DECLARE_FEATURE(kSignInButtonNoAvatar);

// Returns whether the sign-in button without avatar is enabled.
bool IsSignInButtonNoAvatarEnabled();

// Feature flag to enable background customization on the NTP.
BASE_DECLARE_FEATURE(kNTPBackgroundCustomization);

// Checks if background customization is enabled on the NTP.
bool IsNTPBackgroundCustomizationEnabled();

// Feature flag to control whether default status API check and reporting are
// enabled.
BASE_DECLARE_FEATURE(kRunDefaultStatusCheck);

// Returns whether `kRunDefaultStatusCheck` is enabled.
bool IsRunDefaultStatusCheckEnabled();

// Feature flag to have the tab group visually contained.
BASE_DECLARE_FEATURE(kContainedTabGroup);

// Whether the feature associated with contained tab group is enabled.
bool IsContainedTabGroupEnabled();

// Feature flag to have more color for the tab groups.
BASE_DECLARE_FEATURE(kColorfulTabGroup);

// Whether the feature associated with colorful tab group is enabled.
bool IsColorfulTabGroupEnabled();

// Feature flag to highlight the app's features during the FRE.
BASE_DECLARE_FEATURE(kBestOfAppFRE);

// Whether the feature to highlight the app's features during the FRE is
// enabled.
bool IsBestOfAppFREEnabled();

// Whether the Guided Tour variant of `kBestOfAppFRE` is enabled.
bool IsBestOfAppGuidedTourEnabled();

// Whether the Lens Interactive Promo variant of `kBestOfAppFRE` is enabled.
bool IsBestOfAppLensInteractivePromoEnabled();

// Feature flag to include GWS variations in feedback.
BASE_DECLARE_FEATURE(kFeedbackIncludeGWSVariations);

// Whether the feature to include GWS variations in feedback is enabled.
bool IsFeedbackIncludeGWSVariationsEnabled();

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_FEATURES_H_
