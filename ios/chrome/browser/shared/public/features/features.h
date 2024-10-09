// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_FEATURES_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_FEATURES_H_

#import <Foundation/Foundation.h>

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/notifications_promo_view_constants.h"

namespace base {
class TimeDelta;
}  // namespace base

// Feature flag to enable personalized messaging for Default Browser First Run,
// Set Up List, and video promos.
BASE_DECLARE_FEATURE(kSegmentedDefaultBrowserPromo);

// Whether personalized messaging for Default Browser First Run, Set Up List,
// and video promos is enabled.
bool IsSegmentedDefaultBrowserPromoEnabled();

// Feature flag to enable the Keyboard Accessory Upgrade.
BASE_DECLARE_FEATURE(kIOSKeyboardAccessoryUpgrade);

// Feature flag to enable the Keyboard Accessory Upgrade with a shorter manual
// fill menu.
BASE_DECLARE_FEATURE(kIOSKeyboardAccessoryUpgradeShortManualFillMenu);

// Test-only: Feature flag used to verify that EG2 can trigger flags. Must be
// always disabled by default, because it is used to verify that enabling
// features in tests works.
BASE_DECLARE_FEATURE(kTestFeature);

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

// A parameter defining the duration of user inactivity required before
// displaying Safety Check push notifications.
extern const char kSafetyCheckNotificationsUserInactiveThreshold[];

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

// Feature flag to enable Shared Highlighting (Link to Text).
BASE_DECLARE_FEATURE(kSharedHighlightingIOS);

// Feature flag to enable Share button in web context menu in iOS.
BASE_DECLARE_FEATURE(kShareInWebContextMenuIOS);

// Feature flag to enable the modern tabstrip.
BASE_DECLARE_FEATURE(kModernTabStrip);

// Feature parameters for `kModernTabStrip`feature. If no parameter is set,
// `kModernTabStripNTBDynamicParam` will be used.
extern const char kModernTabStripParameterName[];
extern const char kModernTabStripNTBDynamicParam[];
extern const char kModernTabStripNTBStaticParam[];

// Feature parameters for V2 of Modern Tab Strip.
extern const char kModernTabStripCloserNTB[];
extern const char kModernTabStripDarkerBackground[];
extern const char kModernTabStripNTBNoBackground[];
extern const char kModernTabStripBlackBackground[];
extern const char kModernTabStripBiggerNTB[];

// Feature parameters for V3 of Modern Tab Strip.
extern const char kModernTabStripDarkerBackgroundV3[];
extern const char kModernTabStripCloseButtonsVisible[];
extern const char kModernTabStripInactiveTabsHighContrast[];
extern const char kModernTabStripHighContrastNTB[];

// Feature flag that allows external apps to show default browser settings.
BASE_DECLARE_FEATURE(kDefaultBrowserIntentsShowSettings);

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

// Feature flag to enable the non-modal DB promo cooldown refactor separating
// the cooldown periods for full screen and non-modal promos, as well as
// Finchable cooldown period for non-modal promos.
BASE_DECLARE_FEATURE(kNonModalDefaultBrowserPromoCooldownRefactor);

// The default param value for the non-modal promo cooldown period, in days,
// overridable through Finch.
extern const base::FeatureParam<int>
    kNonModalDefaultBrowserPromoCooldownRefactorParam;

// Feature flag to hide search web in the edit menu.
BASE_DECLARE_FEATURE(kIOSEditMenuHideSearchWeb);

// Feature flag to use direct upload for Lens searches.
BASE_DECLARE_FEATURE(kIOSLensUseDirectUpload);

// Feature flag to enable the Lens entrypoint in the home screen widget.
BASE_DECLARE_FEATURE(kEnableLensInHomeScreenWidget);

// Feature flag to enable the color Lens and voice icons in the home screen
// widget.
BASE_DECLARE_FEATURE(kEnableColorLensAndVoiceIconsInHomeScreenWidget);

// Feature flag to enable the Lens entrypoint in the keyboard.
BASE_DECLARE_FEATURE(kEnableLensInKeyboard);

// Feature flag to enable the Lens entrypoint in the new tab page.
BASE_DECLARE_FEATURE(kEnableLensInNTP);

// Feature flag to enable the Lens "Search copied image" omnibox entrypoint.
BASE_DECLARE_FEATURE(kEnableLensInOmniboxCopiedImage);

// Feature flag to enable the Lens "Search copied image" omnibox entrypoint.
BASE_DECLARE_FEATURE(kEnableLensOverlay);
extern const base::NotFatalUntil kLensOverlayNotFatalUntil;

// Feature to enable force showing the lens overlay onboarding screen.
BASE_DECLARE_FEATURE(kLensOverlayForceShowOnboardingScreen);

// Feature flag to enable UITraitCollection workaround for fixing incorrect
// trait propagation.
BASE_DECLARE_FEATURE(kEnableTraitCollectionWorkAround);

// Feature flag to enable duplicate NTP cleanup.
BASE_DECLARE_FEATURE(kRemoveExcessNTPs);

// Feature flag to enable shortened instruction to turn on Password AutoFill for
// Chrome.
BASE_DECLARE_FEATURE(kEnableShortenedPasswordAutoFillInstruction);

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

// Feature flag enabling Save to Drive.
BASE_DECLARE_FEATURE(kIOSSaveToDrive);

// Feature flag enabling Save to Photos.
BASE_DECLARE_FEATURE(kIOSSaveToPhotos);

// Feature flag enabling a fix for the Download manager mediator.
BASE_DECLARE_FEATURE(kIOSDownloadNoUIUpdateInBackground);

// Feature flag to enable feed background refresh.
// Use IsFeedBackgroundRefreshEnabled() instead of this constant directly.
BASE_DECLARE_FEATURE(kEnableFeedBackgroundRefresh);

// Feature flag to enable the Following feed in the NTP.
// Use IsWebChannelsEnabled() instead of this constant directly.
BASE_DECLARE_FEATURE(kEnableWebChannels);

// Feature flag to disable the feed.
BASE_DECLARE_FEATURE(kEnableFeedAblation);

// Feature flag to enable the Follow UI update.
BASE_DECLARE_FEATURE(kEnableFollowUIUpdate);

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

// Feature flag to enable the Large Fakebox design changes.
BASE_DECLARE_FEATURE(kIOSLargeFakebox);

// Feature flag to enable a more stable fullscreen.
BASE_DECLARE_FEATURE(kFullscreenImprovement);

// Feature flag to enable Tab Groups on iPad.
BASE_DECLARE_FEATURE(kTabGroupsIPad);

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

// Feature flag to enable a new illustration in the sync opt-in promotion view.
BASE_DECLARE_FEATURE(kNewSyncOptInIllustration);

// Whether the kNewSyncOptInIllustration feature is enabled.
bool IsNewSyncOptInIllustration();

// Feature flag to disable Lens LVF features.
BASE_DECLARE_FEATURE(kDisableLensCamera);

// Feature flag to enable color icons in the Omnibox.
BASE_DECLARE_FEATURE(kOmniboxColorIcons);

// Feature flag that allows clearing data for managed users signing out.
BASE_DECLARE_FEATURE(kClearDeviceDataOnSignOutForManagedUsers);

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

// Whether feed background refresh is enabled and the capability was enabled at
// startup.
bool IsFeedBackgroundRefreshEnabled();

// Whether feed background refresh capability is enabled. Returns the value in
// NSUserDefaults set by
// `SaveFeedBackgroundRefreshCapabilityEnabledForNextColdStart()`. This is used
// because registering for background refreshes must happen early in app
// initialization and FeatureList is not yet available. Enabling or disabling
// background refresh features will always take effect after two cold starts
// after the feature has been changed on the server (once for the Finch
// configuration, and another for reading the stored value from NSUserDefaults).
// This function always returns false if the `IOS_BACKGROUND_MODE_ENABLED`
// buildflag is not defined.
bool IsFeedBackgroundRefreshCapabilityEnabled();

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

// Returns true if the user should receive a local notification when a feed
// background refresh is completed. Background refresh completion notifications
// are only enabled by Experimental Settings.
bool IsFeedBackgroundRefreshCompletedNotificationEnabled();

// Whether the Following feed should also be refreshed in the background.
bool IsFollowingFeedBackgroundRefreshEnabled();

// Whether the background refresh schedule should be driven by server values.
bool IsServerDrivenBackgroundRefreshScheduleEnabled();

// Whether a new refresh should be scheduled after completion of a previous
// background refresh.
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

// Returns true when the IOSLargeFakebox feature is enabled.
bool IsIOSLargeFakeboxEnabled();

// Whether or not the kIOSKeyboardAccessoryUpgrade feature is enabled.
bool IsKeyboardAccessoryUpgradeEnabled();

// Whether or not the kIOSKeyboardAccessoryUpgradeShortManualFillMenu feature is
// enabled.
bool IsKeyboardAccessoryUpgradeWithShortManualFillMenuEnabled();

// Feature for the Magic Stack.
BASE_DECLARE_FEATURE(kMagicStack);

// Feature that enables tab resumption.
BASE_DECLARE_FEATURE(kTabResumption);

// Feature that enables enhancements for Tab Resumption.
BASE_DECLARE_FEATURE(kTabResumption1_5);

// A parameter to indicate whether the Tab resumption tile should have a see
// more button.
extern const char kTR15SeeMoreButtonParam[];

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

// Feature that enables tab resumption 2.0.
BASE_DECLARE_FEATURE(kTabResumption2);

// Feature that enables tab resumption 2.0 reason bubble.
BASE_DECLARE_FEATURE(kTabResumption2Reason);

// A parameter to indicate whether the Most Visited Tiles should be in the Magic
// Stack.
extern const char kMagicStackMostVisitedModuleParam[];

// A parameter representing how much to reduce the NTP top space margin. If it
// is negative, it will increase the top space margin.
extern const char kReducedSpaceParam[];

// A parameter representing whether modules should not be added to the Magic
// Stack if their content is irrelevant.
extern const char kHideIrrelevantModulesParam[];

// A parameter representing how many days before showing the compacted Set Up
// List module in the Magic Stack.
extern const char kSetUpListCompactedTimeThresholdDays[];

// A parameter to indicate whether the native UI is enabled for the discover
// feed.
// TODO(crbug.com/40246814): Remove this.
extern const char kDiscoverFeedIsNativeUIEnabled[];

// Feature parameters for the tab resumption feature. If no parameter is set,
// the default (most recent tab only) will be used.
extern const char kTabResumptionParameterName[];
extern const char kTabResumptionMostRecentTabOnlyParam[];
extern const char kTabResumptionAllTabsParam[];

// Feature parameters for the tab resumption feature. The threshold for tabs
// fetched from sync in seconds. Default to 12 hours.
extern const char kTabResumptionThresholdParameterName[];

// Whether the tab resumption feature is enabled.
bool IsTabResumptionEnabled();

// Whether the tab resumption feature is enabled in 2.0 version. Implies
// `IsTabResumptionEnabled`.
bool IsTabResumption2_0Enabled();

// Whether to show the reason bubble for Tab resumption.
bool IsTabResumption2ReasonEnabled();

// Whether the tab resumption feature is enabled for most recent tab only.
bool IsTabResumptionEnabledForMostRecentTabOnly();

// Whether the tab resumption enhancements feature is enabled.
bool IsTabResumption1_5Enabled();

// Whether the tab resumption with see more button is enabled.
bool IsTabResumption1_5SeeMoreEnabled();

// Whether the tab resumption with salient images for distant tabs (or fallback
// for local tabs) is enabled.
bool IsTabResumptionImagesSalientEnabled();

// Whether the tab resumption with salient images for local tabs is enabled.
bool IsTabResumptionImagesThumbnailsEnabled();

// Convenience method for determining the tab resumption time threshold for
// X-Devices tabs only.
const base::TimeDelta TabResumptionForXDevicesTimeThreshold();

// Whether the Most Visited Sites should be put into the Magic Stack.
bool ShouldPutMostVisitedSitesInMagicStack();

// How much the NTP top margin should be reduced by for the Magic Stack design.
double ReducedNTPTopMarginSpaceForMagicStack();

// Whether modules should not be added to the Magic Stack if their content is
// irrelevant.
bool ShouldHideIrrelevantModules();

// How many days before showing the Compacted Set Up List module configuration
// in the Magic Stack.
int TimeUntilShowingCompactedSetUpList();

// Kill switch for disabling the navigations when the application is in
// foreground inactive state after opening an external app.
BASE_DECLARE_FEATURE(kInactiveNavigationAfterAppLaunchKillSwitch);

// Feature flag to enable Tips Notifications.
BASE_DECLARE_FEATURE(kIOSTipsNotifications);

// Feature param to specify how much time should elapse before a Tip
// notification should trigger for an unclassified user.
extern const char kIOSTipsNotificationsUnknownTriggerTimeParam[];
// Feature param to specify how much time should elapse before a Tip
// notification should trigger, for an "Active Seeker" user.
extern const char kIOSTipsNotificationsActiveSeekerTriggerTimeParam[];
// Feature param to specify how much time should elapse before a Tip
// notification should trigger, for a "Less Engaged" user.
extern const char kIOSTipsNotificationsLessEngagedTriggerTimeParam[];

// Feature param containing a bitfield to specify which notifications should be
// enabled. Bits are assigned based on the enum `TipsNotificationType`.
extern const char kIOSTipsNotificationsEnabledParam[];

// Feature param containing an integer that chooses from a few options for
// the order that the notifications would be sent in.
extern const char kIOSTipsNotificationsOrderParam[];

// Feature param containing an integer that configures the
// `TipsNotificationClient` to stop requesting notifications if the user
// dismisses this number of notifications in a row. Setting this to zero will
// disable this limit.
extern const char kIOSTipsNotificationsDismissLimitParam[];

// Helper for whether Tips Notifications are enabled.
bool IsIOSTipsNotificationsEnabled();

// Feature flag to disable fullscreen scrolling logic.
BASE_DECLARE_FEATURE(kDisableFullscreenScrolling);

// Convenience method for determining if Pinned Tabs is enabled.
// The Pinned Tabs feature is fully enabled on iPhone and disabled on iPad.
bool IsPinnedTabsEnabled();

// Feature flag for caching the ios module ranker.
BASE_DECLARE_FEATURE(kSegmentationPlatformIosModuleRankerCaching);

// Whether the Segmentation Tips Manager is enabled for Chrome iOS.
bool IsSegmentationTipsManagerEnabled();

// Feature flag for default browser promo experimental string for iPad.
BASE_DECLARE_FEATURE(kDefaultBrowserPromoIPadExperimentalString);

// Returns `YES` if the title and subtitle should be tailored for iPad.
BOOL UseIPadTailoredStringForDefaultBrowserPromo();

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

// Feature that enables personalization of the Home surface.
BASE_DECLARE_FEATURE(kHomeCustomization);

// Returns true if Home Customization is enabled.
bool IsHomeCustomizationEnabled();

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

// Feature to enable the removal of the image in the rich IPH bubble.
BASE_DECLARE_FEATURE(kRichBubbleWithoutImage);

bool IsRichBubbleWithoutImageEnabled();

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
BASE_DECLARE_FEATURE(kSeparateProfilesForManagedAccounts);

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_FEATURES_H_
