// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_FEATURES_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_FEATURES_H_

#import <Foundation/Foundation.h>

#include "Availability.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/notifications_promo_view_constants.h"

namespace base {
class TimeDelta;
}  // namespace base

// Feature flag to enable the Keyboard Accessory Upgrade.
BASE_DECLARE_FEATURE(kIOSKeyboardAccessoryUpgrade);

// Test-only: Feature flag used to verify that EG2 can trigger flags. Must be
// always disabled by default, because it is used to verify that enabling
// features in tests works.
BASE_DECLARE_FEATURE(kTestFeature);

// Feature to add the Safety Check module to the Magic Stack.
BASE_DECLARE_FEATURE(kSafetyCheckMagicStack);

// A parameter representing how many hours must elapse before the Safety Check
// is automatically run in the Magic Stack.
extern const char kSafetyCheckMagicStackAutorunHoursThreshold[];

// How many hours between each autorun of the Safety Check in the Magic Stack.
const base::TimeDelta TimeDelayForSafetyCheckAutorun();

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

// Feature flag to enable revamped Incognito NTP page.
BASE_DECLARE_FEATURE(kIncognitoNtpRevamp);

// Feature flag that allows external apps to show default browser settings.
BASE_DECLARE_FEATURE(kDefaultBrowserIntentsShowSettings);

// Feature flag to log metrics for the edit menu.
BASE_DECLARE_FEATURE(kIOSBrowserEditMenuMetrics);

// Docking Promo experiment variations.

// A parameter representing the experimental arm for when the Docking Promo is
// displayed: during the FRE, or after the FRE.
extern const char kIOSDockingPromoExperimentType[];
// A parameter representing how many hours of inactivity are required (for users
// no older than 2 days) before the Docking Promo is shown.
extern const char kIOSDockingPromoNewUserInactiveThresholdHours[];
// A parameter representing how many hours of inactivity are required (for users
// no older than 14 days) before the Docking Promo is shown.
extern const char kIOSDockingPromoOldUserInactiveThresholdHours[];

// Feature flag to enable the Docking Promo.
BASE_DECLARE_FEATURE(kIOSDockingPromo);

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

// Helper function to check if kIOSDockingPromo is enabled.
bool IsDockingPromoEnabled();

// Returns the experiment type for the Docking Promo feature.
DockingPromoDisplayTriggerArm DockingPromoExperimentTypeEnabled();

// For users no older than 2 days, how many hours of inactivity must pass before
// showing the Docking Promo.
int HoursInactiveForNewUsersUntilShowingDockingPromo();

// For users no older than 14 days, how many hours of inactivity must pass
// before showing the Docking Promo.
int HoursInactiveForOldUsersUntilShowingDockingPromo();

// Feature flag to enable the non-modal DB promo cooldown refactor separating
// the cooldown periods for full screen and non-modal promos, as well as
// Finchable cooldown period for non-modal promos.
BASE_DECLARE_FEATURE(kNonModalDefaultBrowserPromoCooldownRefactor);

// The default param value for the non-modal promo cooldown period, in days,
// overridable through Finch.
extern const base::FeatureParam<int>
    kNonModalDefaultBrowserPromoCooldownRefactorParam;

// Feature flag that enables the default browser video promo.
BASE_DECLARE_FEATURE(kDefaultBrowserVideoPromo);

// Feature param under kIOSEditMenuPartialTranslate to disable on incognito.
extern const char kIOSEditMenuPartialTranslateNoIncognitoParam[];
// Feature flag to enable partial translate in the edit menu.
BASE_DECLARE_FEATURE(kIOSEditMenuPartialTranslate);

// Helper function to check if kIOSEditMenuPartialTranslate is enabled and on
// supported OS.
bool IsPartialTranslateEnabled();

// Helper function to check if kIOSEditMenuPartialTranslate is enabled in
// incognito.
bool ShouldShowPartialTranslateInIncognito();

// Feature param under kIOSEditMenuSearchWith to select the title.
extern const char kIOSEditMenuSearchWithTitleParamTitle[];
extern const char kIOSEditMenuSearchWithTitleSearchParam[];
extern const char kIOSEditMenuSearchWithTitleSearchWithParam[];
extern const char kIOSEditMenuSearchWithTitleWebSearchParam[];
// Feature flag to enable search with in the edit menu.
BASE_DECLARE_FEATURE(kIOSEditMenuSearchWith);

// Helper function to check if kIOSEditMenuSearchWith is enabled and on
// supported OS.
bool IsSearchWithEnabled();

// Feature flag to hide search web in the edit menu.
BASE_DECLARE_FEATURE(kIOSEditMenuHideSearchWeb);

// Feature flag that swaps the omnibox textfield implementation.
BASE_DECLARE_FEATURE(kIOSNewOmniboxImplementation);

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

// Feature flag to enable UITraitCollection workaround for fixing incorrect
// trait propagation.
BASE_DECLARE_FEATURE(kEnableTraitCollectionWorkAround);

// Feature flag to enable duplicate NTP cleanup.
BASE_DECLARE_FEATURE(kRemoveExcessNTPs);

// Feature flag to enable shortened instruction to turn on Password AutoFill for
// Chrome.
BASE_DECLARE_FEATURE(kEnableShortenedPasswordAutoFillInstruction);

// Feature flag to enable startup latency improvements.
BASE_DECLARE_FEATURE(kEnableStartupImprovements);

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

// Enables indexing Open tabs items in Spotlight.
BASE_DECLARE_FEATURE(kSpotlightOpenTabsSource);

// Enables indexing Reading List items in Spotlight.
BASE_DECLARE_FEATURE(kSpotlightReadingListSource);

// Enables intent donation for new intent types.
BASE_DECLARE_FEATURE(kSpotlightDonateNewIntents);

// Feature to enable sign-in only flow without device level account.
BASE_DECLARE_FEATURE(kConsistencyNewAccountInterface);

// Whether the flag for consistency new-account interface is enabled.
bool IsConsistencyNewAccountInterfaceEnabled();

// Feature flag to enable the new layout of the NTP omnibox.
BASE_DECLARE_FEATURE(kNewNTPOmniboxLayout);

// Feature param under kBottomOmniboxDefaultSetting to select the default
// setting.
extern const char kBottomOmniboxDefaultSettingParam[];
extern const char kBottomOmniboxDefaultSettingParamTop[];
extern const char kBottomOmniboxDefaultSettingParamBottom[];
extern const char kBottomOmniboxDefaultSettingParamSafariSwitcher[];
// Feature flag to change the default position of the omnibox.
BASE_DECLARE_FEATURE(kBottomOmniboxDefaultSetting);

// Returns true if the bottom omnibox feature is enabled. This does not check
// that the omnibox is currently at the bottom.
bool IsBottomOmniboxSteadyStateEnabled();

// Feature flag to enable the bottom omnibox FRE promo.
BASE_DECLARE_FEATURE(kBottomOmniboxPromoFRE);

// Feature flag to enable the bottom omnibox app-launch promo.
BASE_DECLARE_FEATURE(kBottomOmniboxPromoAppLaunch);

// Feature param under kBottomOmniboxPromoFRE or kBottomOmniboxPromoAppLaunch to
// skip the promo conditions for testing.
extern const char kBottomOmniboxPromoParam[];
extern const char kBottomOmniboxPromoParamForced[];

// Type of bottom omnibox promo.
enum class BottomOmniboxPromoType {
  // kBottomOmniboxPromoFRE.
  kFRE,
  // kBottomOmniboxPromoAppLaunch.
  kAppLaunch,
  // Any promo type.
  kAny,
};

// Whether the bottom omnibox promo of `type` is enabled.
bool IsBottomOmniboxPromoFlagEnabled(BottomOmniboxPromoType type);

// Feature flag to change the default proposed position in omnibox promos.
BASE_DECLARE_FEATURE(kBottomOmniboxPromoDefaultPosition);

// Feature param under kBottomOmniboxPromoDefaultPosition to select the default
// position.
extern const char kBottomOmniboxPromoDefaultPositionParam[];
extern const char kBottomOmniboxPromoDefaultPositionParamTop[];
extern const char kBottomOmniboxPromoDefaultPositionParamBottom[];

// Feature flag to enable region filter for the bottom omnibox promos.
BASE_DECLARE_FEATURE(kBottomOmniboxPromoRegionFilter);

// Feature flag to put all clipboard access onto a background thread. Any
// synchronous clipboard access will always return nil/false.
BASE_DECLARE_FEATURE(kOnlyAccessClipboardAsync);

// Feature flag that enables default browser video in settings experiment.
BASE_DECLARE_FEATURE(kDefaultBrowserVideoInSettings);

// Feature flag to try using the page theme color in the top toolbar
BASE_DECLARE_FEATURE(kThemeColorInTopToolbar);

// Feature flag enabling the Tab Grid to always bounce (even when the content
// fits the screen already).
BASE_DECLARE_FEATURE(kTabGridAlwaysBounce);

// Feature flag enabling tab grid refactoring.
BASE_DECLARE_FEATURE(kTabGridRefactoring);

// Feature flag enabling the tab grid new compositional layout.
BASE_DECLARE_FEATURE(kTabGridCompositionalLayout);

// Whether the Tab Grid should use its compositional layout.
bool IsTabGridCompositionalLayoutEnabled();

// Whether the Safety Check module should be shown in the Magic Stack.
bool IsSafetyCheckMagicStackEnabled();

// Feature flag enabling Save to Drive.
BASE_DECLARE_FEATURE(kIOSSaveToDrive);

// Feature flag enabling Save to Photos.
BASE_DECLARE_FEATURE(kIOSSaveToPhotos);

// Enables the new UIEditMenuInteraction system to be used in place of
// UIMenuController which was deprecated in iOS 16.
// TODO(crbug.com/1489734) Remove Flag once the minimum iOS deployment version
// has been increased to iOS 16.
BASE_DECLARE_FEATURE(kEnableUIEditMenuInteraction);

// Causes the restore shorty and re-signin flows to offer a history opt-in
// screen. This only has any effect if kReplaceSyncPromosWithSignInPromos is
// also enabled.
BASE_DECLARE_FEATURE(kHistoryOptInForRestoreShortyAndReSignin);

// Enables batch upload entry point from the Bookmarks Manager.
// Note: This has no effect if kReplaceSyncPromosWithSignInPromos is not
// enabled.
BASE_DECLARE_FEATURE(kEnableBatchUploadFromBookmarksManager);

// Enables the promo in the Bookmarks Manager or Reading Lists Manager to review
// account settings when these types are disabled. Note: This should only be
// used if kReplaceSyncPromosWithSignInPromos is enabled.
BASE_DECLARE_FEATURE(kEnableReviewAccountSettingsPromo);

// Enables linking account settings in the Privacy Settings page footer for
// signed in non syncing users.
BASE_DECLARE_FEATURE(kLinkAccountSettingsToPrivacyFooter);

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

// Feature flag to enable the live sport card in the Discover feed.
BASE_DECLARE_FEATURE(kDiscoverFeedSportCard);

// Content Push Notifications Variations.
extern const char kContentPushNotificationsExperimentType[];

// Feature flag to enable the content notifications.
BASE_DECLARE_FEATURE(kContentPushNotifications);

// Feature flag to enable the Large Fakebox design changes.
BASE_DECLARE_FEATURE(kIOSLargeFakebox);

// Feature flag to enable hiding the feed and feed header depending on Search
// Engine choice.
BASE_DECLARE_FEATURE(kIOSHideFeedWithSearchChoice);

// Feature flag to enable a more stable fullscreen.
BASE_DECLARE_FEATURE(kFullscreenImprovement);

// Feature flag to enable Tab Groups in Grid.
BASE_DECLARE_FEATURE(kTabGroupsInGrid);

// Whether the Tab Groups should be enabled in the Grid.
bool IsTabGroupInGridEnabled();

// Feature flag to enable the handling of external actions passed to Chrome.
// Enabled by default.
BASE_DECLARE_FEATURE(kIOSExternalActionURLs);

// Feature flag to disable Lens LVF features.
BASE_DECLARE_FEATURE(kDisableLensCamera);

// Feature flag to enable color icons in the Omnibox.
BASE_DECLARE_FEATURE(kOmniboxColorIcons);

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

// Feature param under `kIOSHideFeedWithSearchChoice` to only target the
// feature at certain countries (i.e. only hide the feed when the device is
// from those countries when the search engine is changed).
extern const char kIOSHideFeedWithSearchChoiceTargeted[];

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

// Returns whether the feed hide with search choice feature should be targeted
// only at devices from certain countries.
bool IsIOSHideFeedWithSearchChoiceTargeted();

// Whether the feed is disabled.
bool IsFeedAblationEnabled();

// YES when Follow UI Update is enabled.
bool IsFollowUIUpdateEnabled();

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

// TODO(b/322348322): Remove provisional notifications bypass conditions testing
// flag param. YES when the Content Provisional Push Notifications are enabled
// and the time based conditions should be ignored.
bool IsContentPushNotificationsProvisionalBypass();

// Returns true when the IOSLargeFakebox feature is enabled.
bool IsIOSLargeFakeboxEnabled();

// Returns true when the IOSHideFeedWithSearchChoice feature is enabled.
bool IsIOSHideFeedWithSearchChoiceEnabled();

// Whether or not the kIOSKeyboardAccessoryUpgrade feature is enabled.
bool IsKeyboardAccessoryUpgradeEnabled();

// Feature for the Magic Stack.
BASE_DECLARE_FEATURE(kMagicStack);

// Feature that contains the feed in a module.
BASE_DECLARE_FEATURE(kEnableFeedContainment);

// Feature that enables tab resumption.
BASE_DECLARE_FEATURE(kTabResumption);

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
// TODO(crbug.com/1385512): Remove this.
extern const char kDiscoverFeedIsNativeUIEnabled[];

// Feature parameters for the tab resumption feature. If no parameter is set,
// the default (most recent tab only) will be used.
extern const char kTabResumptionParameterName[];
extern const char kTabResumptionMostRecentTabOnlyParam[];
extern const char kTabResumptionAllTabsParam[];
extern const char kTabResumptionAllTabsOneDayThresholdParam[];

// Whether the feed is contained in a Home module.
bool IsFeedContainmentEnabled();

// The minimum padding between the modules and the screen bounds on the Home
// surface. Relies on `IsFeedContainmentEnabled()` being enabled.
int HomeModuleMinimumPadding();

// Whether the tab resumption feature is enabled.
bool IsTabResumptionEnabled();

// Whether the tab resumption feature is enabled for most recent tab only.
bool IsTabResumptionEnabledForMostRecentTabOnly();

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

// Helper for whether the external action handling flag is enabled.
bool IsExternalActionSchemeHandlingEnabled();

// Kill switch for disabling the navigations when the application is in
// foreground inactive state after opening an external app.
BASE_DECLARE_FEATURE(kInactiveNavigationAfterAppLaunchKillSwitch);

// Feature flag to enable Tips Notifications.
BASE_DECLARE_FEATURE(kIOSTipsNotifications);

// Feature param to specify how much time after the app starts to trigger
// Tips notifications.
extern const char kIOSTipsNotificationsTriggerTimeParam[];

// Feature param containing a bitfield to specify which notifications should be
// enabled. Bits are assigned based on the enum `TipsNotificationType`.
extern const char kIOSTipsNotificationsEnabledParam[];

// Helper for whether Tips Notifications are enabled.
bool IsIOSTipsNotificationsEnabled();

// Feature flag to use a UICollectionView for the Magic Stack.
BASE_DECLARE_FEATURE(kIOSMagicStackCollectionView);

// Returns true if the MagicStack UICollectionView implementation is enabled.
bool IsIOSMagicStackCollectionViewEnabled();

// Feature flag to disable fullscreen scrolling logic.
BASE_DECLARE_FEATURE(kDisableFullscreenScrolling);

// Feature flag to prefetch system capabilities on first run.
BASE_DECLARE_FEATURE(kPrefetchSystemCapabilitiesOnFirstRun);

// Returns true if the system capabilities are prefetched on first run.
bool IsPrefetchingSystemCapabilitiesOnFirstRun();

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_FEATURES_H_
