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

// Test-only: Feature flag used to verify that EG2 can trigger flags. Must be
// always disabled by default, because it is used to verify that enabling
// features in tests works.
BASE_DECLARE_FEATURE(kTestFeature);

// Killswitch to control how Safety Checks are automatically triggered.
// If enabled, the Safety Check Manager can independently initiate Safety
// Checks. If disabled, automatic Safety Check runs must be triggered through
// the Safety Check module in the Magic Stack.
BASE_DECLARE_FEATURE(kSafetyCheckAutorunByManagerKillswitch);

// Killswitch for conditionally hiding the Safety Check module in the Magic
// Stack if no issues are found.
BASE_DECLARE_FEATURE(kSafetyCheckModuleHiddenIfNoIssuesKillswitch);

// Feature to enable the refactored implementation of the `OmahaService`, using
// new `OmahaServiceObserver`(s) for Omaha clients. Acts as a killswitch.
BASE_DECLARE_FEATURE(kOmahaServiceRefactor);

// Feature flag to enable Shared Highlighting (Link to Text).
BASE_DECLARE_FEATURE(kSharedHighlightingIOS);

// Feature flag to log metrics for the edit menu.
BASE_DECLARE_FEATURE(kIOSBrowserEditMenuMetrics);

// Feature flag to enable the custom file upload menu.
BASE_DECLARE_FEATURE(kIOSCustomFileUploadMenu);

// Docking Promo experiment variations.
extern const char kIOSDockingPromoV2VariationParam[];
extern const char kIOSDockingPromoV2VariationHeader1[];
extern const char kIOSDockingPromoV2VariationHeader2[];
extern const char kIOSDockingPromoV2VariationHeader3[];

// Feature flag to enable the Docking Promo V2.
BASE_DECLARE_FEATURE(kIOSDockingPromoV2);

// Helper function to check if `kIOSDockingPromoV2` is enabled.
bool IsDockingPromoV2Enabled();

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

// Whether to enable loading AIM in the lens result page.
BASE_DECLARE_FEATURE(kLensLoadAIMInLensResultPage);

// Feature to allow landscape support of lens overlay.
BASE_DECLARE_FEATURE(kLensOverlayEnableLandscapeCompatibility);

// Feature flag to add lens overlay navigation to history.
BASE_DECLARE_FEATURE(kLensOverlayNavigationHistory);

// Feature flag to add a custom bottom sheet presentation Lens results.
BASE_DECLARE_FEATURE(kLensOverlayCustomBottomSheet);

// Feature flag to check headers for lens searches.
BASE_DECLARE_FEATURE(kLensSearchHeadersCheckEnabled);

// Variations of MIA NTP entrypoint.
extern const char kNTPMIAEntrypointParam[];
extern const char kNTPMIAEntrypointParamOmniboxContainedSingleButton[];
extern const char kNTPMIAEntrypointParamOmniboxContainedInline[];
extern const char kNTPMIAEntrypointParamOmniboxContainedEnlargedFakebox[];
extern const char kNTPMIAEntrypointParamEnlargedFakeboxNoIncognito[];
extern const char kNTPMIAEntrypointParamAIMInQuickActions[];

// Feature flag to change the MIA entrypoint in NTP. Applies to en-US locales
// only.
BASE_DECLARE_FEATURE(kNTPMIAEntrypoint);
// Like above, but applies regardless of client's locale.
BASE_DECLARE_FEATURE(kNTPMIAEntrypointAllLocales);

// Used to gate the immersive SRP in the Composebox.
BASE_DECLARE_FEATURE(kComposeboxImmersiveSRP);

// Variations of Composebox tab picker.
extern const char kComposeboxTabPickerVariationParam[];
extern const char kComposeboxTabPickerVariationParamCachedAPC[];
extern const char kComposeboxTabPickerVariationParamOnFlightAPC[];

// Feature flag for the tab picker in the Composebox.
BASE_DECLARE_FEATURE(kComposeboxTabPickerVariation);

// Returns true is we should use cached APCs in the Composebox.
bool IsComposeboxTabPickerCachedAPCEnabled();

// Variations of Composebox.
extern const char kComposeboxParam[];
extern const char kComposeboxParamAllOmniboxEntrypoints[];

// Feature for the DRS prototype.
BASE_DECLARE_FEATURE(kOmniboxDRSPrototype);

// Feature flag to enable UITraitCollection workaround for fixing incorrect
// trait propagation.
BASE_DECLARE_FEATURE(kEnableTraitCollectionWorkAround);

// Feature flag to enable duplicate NTP cleanup.
BASE_DECLARE_FEATURE(kRemoveExcessNTPs);

// Feature flag / Kill Switch for TCRex.
BASE_DECLARE_FEATURE(kTCRexKillSwitch);

// When enabled uses new transitions in the TabGrid.
BASE_DECLARE_FEATURE(kTabGridNewTransitions);

// Feature flag for the tab grid drag and drop functionality.
BASE_DECLARE_FEATURE(kTabGridDragAndDrop);

// YES if the tab grid drag and drop feature is enabled.
bool IsTabGridDragAndDropEnabled();

// Whether the new tab grid tabs transitions should be enabled.
bool IsNewTabGridTransitionsEnabled();

// When enabled, a Tab Group button will appear in the overflow menu.
BASE_DECLARE_FEATURE(kTabGroupInOverflowMenu);

// When enabled, a Tab Group button will appear in the Tab Icon context menu.
BASE_DECLARE_FEATURE(kTabGroupInTabIconContextMenu);

// When enabled, a "New Tab Button" will be added to the Tab Group recall
// surface.
BASE_DECLARE_FEATURE(kTabRecallNewTabGroupButton);

// When enabled, an overflow menu will replace the edit menu on the GTS.
BASE_DECLARE_FEATURE(kTabSwitcherOverflowMenu);

// Feature to enable force showing the Contextual Panel entrypoint.
BASE_DECLARE_FEATURE(kContextualPanelForceShowEntrypoint);

bool IsContextualPanelForceShowEntrypointEnabled();

// Feature flag to control the maximum amount of non-modal DB promo impressions
// server-side. Enabled by default to always have a default impression limit
// value.
BASE_DECLARE_FEATURE(kNonModalDefaultBrowserPromoImpressionLimit);

// The default param value for the non-modal DB promo impression limit,
// overridable through Finch. The associated histogram supports a maximum of 10
// impressions.
extern const base::FeatureParam<int>
    kNonModalDefaultBrowserPromoImpressionLimitParam;

// Feature flag the "Hide Toolbar" button in the overflow menu.
BASE_DECLARE_FEATURE(kHideToolbarsInOverflowMenu);

// Flag to hide voice and lens actions in fusebox.
BASE_DECLARE_FEATURE(kHideFuseboxVoiceLensActions);

// Whether the Safety Check Manager can automatically trigger Safety Checks.
bool IsSafetyCheckAutorunByManagerEnabled();

// Whether the Safety Check module is hidden when no issues are found.
bool ShouldHideSafetyCheckModuleIfNoIssues();

// Whether the refactored implementation of the `OmahaService` is enabled.
bool IsOmahaServiceRefactorEnabled();

// Feature flag enabling Choose from Drive.
// TODO(crbug.com/473788390): Clean-up feature once file upload menu is ready.
BASE_DECLARE_FEATURE(kIOSChooseFromDrive);

// Feature flag enabling Choose from Drive for signed out users.
BASE_DECLARE_FEATURE(kIOSChooseFromDriveSignedOut);

// Feature flag enabling dates long press to enter the Create Calendar event
// ExperienceKit for signed out users.
BASE_DECLARE_FEATURE(kIOSDateToCalendarSignedOut);

// Feature flag enabling a fix for the Download manager mediator.
BASE_DECLARE_FEATURE(kIOSDownloadNoUIUpdateInBackground);

// Feature flag enabling the client folder implementation of Save to Drive.
BASE_DECLARE_FEATURE(kIOSSaveToDriveClientFolder);

// Feature flag enabling the save to drive feature for signed out users.
BASE_DECLARE_FEATURE(kIOSSaveToDriveSignedOut);

// Feature flag enabling the save to photos feature for signed out users.
BASE_DECLARE_FEATURE(kIOSSaveToPhotosSignedOut);

// Feature flag to enable feed background refresh.
// Use IsFeedBackgroundRefreshEnabled() instead of this constant directly.
BASE_DECLARE_FEATURE(kEnableFeedBackgroundRefresh);

// Feature flag to disable the feed.
BASE_DECLARE_FEATURE(kEnableFeedAblation);

// Content Push Notifications Variations.
extern const char kContentPushNotificationsExperimentType[];

// Feature flag to enable the content notifications.
BASE_DECLARE_FEATURE(kContentPushNotifications);

// Feature flag to enable Content Notification Provisional without any
// conditions.
BASE_DECLARE_FEATURE(kContentNotificationProvisionalIgnoreConditions);

// True if Content Notification Provisional is enabled without any conditions.
bool IsContentNotificationProvisionalIgnoreConditions();

// Flag to override delivered NAUs.
BASE_DECLARE_FEATURE(kContentNotificationDeliveredNAU);

// Parameter value for the max number of delivered NAUs to be sent per session.
extern const char kDeliveredNAUMaxPerSession[];

// Feature flag to enable a new illustration in the sync opt-in promotion view.
BASE_DECLARE_FEATURE(kNewSyncOptInIllustration);

// Whether the kNewSyncOptInIllustration feature is enabled.
bool IsNewSyncOptInIllustration();

// Feature flag to disable Lens LVF features.
BASE_DECLARE_FEATURE(kDisableLensCamera);

// Feature flag that allows the Auto-deletion feature to clear all downloaded
// files scheduled for deletion on every application startup, regardless of when
// the file was downloaded. This feature is intended for testing-only.
BASE_DECLARE_FEATURE(kDownloadAutoDeletionClearFilesOnEveryStartup);

bool isDownloadAutoDeletionTestingFeatureEnabled();

// YES when the Downloads Auto Deletion feature is enabled.
BASE_DECLARE_FEATURE(kDownloadAutoDeletionFeatureEnabled);

// Whether the kDownloadAutoDeletion feature is enabled.
bool IsDownloadAutoDeletionFeatureEnabled();

// Download List UI feature constants and types.
extern const char kDownloadListUITypeParam[];

// Enum defining the available Download List UI types.
// IMPORTANT: These values must match the parameter strings in about_flags.mm
enum class DownloadListUIType {
  kDefaultUI = 0,  // Use the default iOS download list UI
  kCustomUI = 1,   // Use a custom download list UI implementation
};

// Returns the currently configured Download List UI type based on feature
// parameters.
DownloadListUIType CurrentDownloadListUIType();

// Returns true if the Download List feature is enabled.
bool IsDownloadListEnabled();

// Feature flag to control the download list UI type.
BASE_DECLARE_FEATURE(kDownloadList);

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

// Whether the liquid glass effect is enabled. Returns true on iOS 26+ if the
// Keyboard Accessory Upgrade feature is enabled (pre KA upgrade code is about
// to be deprecated, so we're not adding liquid glass support to it). Returns
// false otherwise.
bool IsLiquidGlassEffectEnabled();

// Feature flag to enable the default input accessory view.
BASE_DECLARE_FEATURE(kIOSKeyboardAccessoryDefaultView);

// Feature flag to enable the two-bubble design for the Keyboard Accessory view.
BASE_DECLARE_FEATURE(kIOSKeyboardAccessoryTwoBubble);

// Returns true if the default input accessory view is enabled.
bool IsIOSKeyboardAccessoryDefaultViewEnabled();

// Returns true if the two-bubble design for the keyboard accessory view is
// enabled.
bool IsIOSKeyboardAccessoryTwoBubbleEnabled();

// Name of the parameter for kIOSKeyboardAccessoryTwoBubble to use a keyboard
// icon.
inline constexpr char kIOSKeyboardAccessoryTwoBubbleKeyboardIconParamName[] =
    "use_keyboard_icon";

// Feature param for kIOSKeyboardAccessoryTwoBubble to use a keyboard icon.
BASE_DECLARE_FEATURE_PARAM(bool,
                           kIOSKeyboardAccessoryTwoBubbleKeyboardIconParam);

// Kill switch for disabling the navigations when the application is in
// foreground inactive state after opening an external app.
BASE_DECLARE_FEATURE(kInactiveNavigationAfterAppLaunchKillSwitch);

// Convenience method for determining if Pinned Tabs is enabled.
// The Pinned Tabs feature is fully enabled on iPhone and disabled on iPad.
bool IsPinnedTabsEnabled();

// Feature flag for caching the ios module ranker.
BASE_DECLARE_FEATURE(kSegmentationPlatformIosModuleRankerCaching);

// Feature flag to enable app background refresh.
// Use IsAppBackgroundRefreshEnabled() instead of this constant directly.
BASE_DECLARE_FEATURE(kEnableAppBackgroundRefresh);

// Whether app background refresh is enabled.
bool IsAppBackgroundRefreshEnabled();

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

// Feature param for kSeparateProfilesForManagedAccountsForceMigration to
// specify how much time to wait before force-migrating the primary managed
// account to its own separate profile.
extern const base::FeatureParam<base::TimeDelta>
    kMultiProfileMigrationGracePeriod;

// Feature flag to control force-migrating the primary managed account to its
// own separate profile.
BASE_DECLARE_FEATURE(kSeparateProfilesForManagedAccountsForceMigration);

// Feature to control resyncing the omaha ping timer on foregrounding.
BASE_DECLARE_FEATURE(kOmahaResyncTimerOnForeground);

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

// Variations of expanded Setup List.
extern const char kIOSExpandedSetupListVariationParam[];
extern const char kIOSExpandedSetupListVariationParamSafariImport[];
extern const char kIOSExpandedSetupListVariationParamBackgroundCustomization[];
extern const char kIOSExpandedSetupListVariationParamAllExceptCPE[];
extern const char kIOSExpandedSetupListVariationParamAll[];

// Feature flag to add new items in the Setup List.
BASE_DECLARE_FEATURE(kIOSExpandedSetupList);

bool IsIOSExpandedSetupListEnabled();

// Feature flag to enable Expanded Tips.
BASE_DECLARE_FEATURE(kIOSExpandedTips);

// Feature param containing a comma separated list of integers that represent
// cases of the `TipsNotificationType` enum.
extern const char kIOSExpandedTipsOrderParam[];

// Returns whether `kIOSExpandTips` is enabled.
bool IsIOSExpandedTipsEnabled();

// Feature flag for the one-time default browser notification.
BASE_DECLARE_FEATURE(kIOSOneTimeDefaultBrowserNotification);

// Feature flag to show an alert to the user when only provisiona notifications
// are allowed.
BASE_DECLARE_FEATURE(kProvisionalNotificationAlert);

// Returns whether `kIOSReactivationNotifications` is enabled.
bool IsProvisionalNotificationAlertEnabled();

// Feature to enable different text for the main header text on FRE sign-in
// promo.
BASE_DECLARE_FEATURE(kFRESignInHeaderTextUpdate);
extern const base::FeatureParam<std::string> kFRESignInHeaderTextUpdateParam;
extern const std::string_view kFRESignInHeaderTextUpdateParamArm0;
extern const std::string_view kFRESignInHeaderTextUpdateParamArm1;

// Returns whether 'kFRESignInHeaderTextUpdate' is enabled.
bool FRESignInHeaderTextUpdate();

// Feature to enable different text for the secondary action on FRE sign-in
// promo.
BASE_DECLARE_FEATURE(kFRESignInSecondaryActionLabelUpdate);
extern const base::FeatureParam<std::string>
    kFRESignInSecondaryActionLabelUpdateParam;
extern const std::string_view
    kFRESignInSecondaryActionLabelUpdateParamStaySignedOut;

// Returns whether 'kFRESignInSecondaryActionLabelUpdate' is enabled.
bool FRESignInSecondaryActionLabelUpdate();

// Feature flag to change the button order in the confirmation alerts, placing
// the primary CTA below the secondary button.
BASE_DECLARE_FEATURE(kConfirmationButtonSwapOrder);

// Checks if the button order in the confirmation alerts should be swapped
// (primary button at the bottom), based on the `kConfirmationButtonSwapOrder`
// flag.
bool IsConfirmationButtonSwapOrderEnabled();

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

bool IsFullscreenTransitionSpeedSet();

// Feature flag to changes the speed of the transition to fullscreen.
BASE_DECLARE_FEATURE(kFullscreenTransitionSpeed);

// Feature flag for switching the toolbar UI to an observer-based architecture.
BASE_DECLARE_FEATURE(kRefactorToolbarsSize);

bool IsRefactorToolbarsSize();

// Feature that disables all IPH messages.
BASE_DECLARE_FEATURE(kIPHAblation);

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

// Feature flag to enable background customization on the NTP.
BASE_DECLARE_FEATURE(kNTPBackgroundCustomization);

// The parameter representing the maximum number of recently used NTP
// backgrounds to store.
extern const base::FeatureParam<int> kMaxRecentlyUsedBackgrounds;

// The maximum number of recently used NTP backgrounds to store.
int MaxRecentlyUsedBackgrounds();

// Checks if background customization is enabled on the NTP.
bool IsNTPBackgroundCustomizationEnabled();

// Feature flag to control whether default status API check and reporting are
// enabled.
BASE_DECLARE_FEATURE(kRunDefaultStatusCheck);

// Feature flag to enable the custom color slider on the NTP.
BASE_DECLARE_FEATURE(kNTPBackgroundColorSlider);

// Checks if the custom color slider is enabled on the NTP.
bool IsNTPBackgroundColorSliderEnabled();

// Returns whether `kRunDefaultStatusCheck` is enabled.
bool IsRunDefaultStatusCheckEnabled();

// Feature flag to highlight the app's features during the FRE.
BASE_DECLARE_FEATURE(kBestOfAppFRE);

// Whether the feature to highlight the app's features during the FRE is
// enabled.
bool IsBestOfAppFREEnabled();

// All currently enabled `KBestOfAppFRE` feature variants.
std::vector<std::string> GetBestOfAppFREActiveVariants();

// Whether the Guided Tour variant of `kBestOfAppFRE` is enabled.
bool IsBestOfAppGuidedTourEnabled();

// Whether manual UMA uploads are enabled for the Best Of App feature.
bool IsManualUploadForBestOfAppEnabled();

// Whether the Lens Interactive Promo variant of `kBestOfAppFRE` is enabled.
bool IsBestOfAppLensInteractivePromoEnabled();

// Whether the Lens Animated Promo variant of `kBestOfAppFRE` is enabled.
bool IsBestOfAppLensAnimatedPromoEnabled();

// Whether the `kDefaultBrowserPromoPropensityModel` feature is enabled.
bool IsDefaultBrowserPromoPropensityModelEnabled();

// Feature flag to enable the trusted vault provisional notification.
BASE_DECLARE_FEATURE(kIOSTrustedVaultNotification);

// Returns whether `kIOSTrustedVaultNotification` is enabled.
bool IsIOSTrustedVaultNotificationEnabled();

// Feature flag for the Default Browser off-cycle promo.
BASE_DECLARE_FEATURE(kIOSDefaultBrowserOffCyclePromo);

bool IsDefaultBrowserOffCyclePromoEnabled();

// Feature flag for logging the app install attribution.
BASE_DECLARE_FEATURE(kIOSLogInstallAttribution);

bool IsInstallAttributionLoggingEnabled();

// Feature flag for logging the app install attribution from App Preview.
BASE_DECLARE_FEATURE(kIOSLogAppPreviewInstallAttribution);

bool IsAppPreviewInstallAttributionLoggingEnabled();

// Feature flag for migrating all default browser promos to use the new Default
// Apps iOS settings page.
BASE_DECLARE_FEATURE(kIOSUseDefaultAppsDestinationForPromos);

bool IsDefaultAppsDestinationAvailable();
bool IsUseDefaultAppsDestinationForPromosEnabled();

// Feature flag for a workaround on iOS26 to show edit menu items synchronously.
// Enabled by default. Can be disabled if the bug is fixed on iOS 26.
BASE_DECLARE_FEATURE(kSynchronousEditMenuItems);
bool ShouldShowEditMenuItemsSynchronously();

// Feature flag for tips notifications alternative string experiment.
BASE_DECLARE_FEATURE(kIOSTipsNotificationsAlternativeStrings);
bool IsTipsNotificationsAlternativeStringsEnabled();

// Name of the parameter that controls tips notifications alternative string
// version.
extern const char kTipsNotificationsAlternativeStringVersion[];

// Tips notifications alternative string version for
// ```kIOSTipsNotificationsAlternativeStrings``` experiment.
enum class TipsNotificationsAlternativeStringVersion {
  kDefault = 0,
  kAlternative1 = 1,
  kAlternative2 = 2,
  kAlternative3 = 3,
};

// Returns the string alternative version for
// ```kIOSTipsNotificationsAlternativeStrings``` experiment.
TipsNotificationsAlternativeStringVersion
GetTipsNotificationsAlternativeStringVersion();

// Feature for applying cross device settings through the Synced Set Up
// experience.
BASE_DECLARE_FEATURE(kIOSSyncedSetUp);

// Returns true if `kIOSSyncedSetUp` is enabled.
bool IsSyncedSetUpEnabled();

// Name of the Finch parameter controlling the maximum number of impressions
// allowed for the Synced Set Up promo.
extern const char kSyncedSetUpImpressionLimit[];

// Returns the maximum number of impressions allowed for the Synced Set Up
// promo, as specified by the `kSyncedSetUpImpressionLimit` Finch parameter.
int GetSyncedSetUpImpressionLimit();

// Enables the DisableKeyboardAccessory feature.
BASE_DECLARE_FEATURE(kDisableKeyboardAccessory);

// Variations for DisableKeyboardAccessory feature.
extern const char kDisableKeyboardAccessoryParam[];
extern const char kDisableKeyboardAccessoryOnlySymbols[];
extern const char kDisableKeyboardAccessoryOnlyFeatures[];
extern const char kDisableKeyboardAccessoryCompletely[];

// Enables the keyboard accessory in the Fusebox.
BASE_DECLARE_FEATURE(kEnableFuseboxKeyboardAccessory);
extern const char kEnableFuseboxKeyboardAccessoryParam[];
extern const char kEnableFuseboxKeyboardAccessoryOnlySymbols[];
extern const char kEnableFuseboxKeyboardAccessoryOnlyFeatures[];
extern const char kEnableFuseboxKeyboardAccessoryBoth[];

// Returns true if keyboard accessory is enabled.
bool ShouldShowKeyboardAccessory();
// Returns true if the symbols :/- and .com in the keyboard accessory are
// enabled.
bool ShouldShowKeyboardAccessorySymbols();
// Returns true if lens and voice search can be shown in the keyboard accessory.
bool ShouldShowKeyboardAccessoryFeatures();

// Enables the LocationBarBadgeMigration feature.
BASE_DECLARE_FEATURE(kLocationBarBadgeMigration);

// Returns true if the LocationBarBadgeMigration feature is enabled.
bool IsLocationBarBadgeMigrationEnabled();

// Enables the Composebox feature.
BASE_DECLARE_FEATURE(kComposeboxIOS);

// Returns true if the Composebox feature is enabled.
bool IsComposeboxIOSEnabled();

// The feature to enable or disable the group color on the tab group and tab
// grid surfaces.
BASE_DECLARE_FEATURE(kTabGroupColorOnSurface);

// Returns true if the TabGroupColorOnSurface feature is enabled.
bool IsTabGroupColorOnSurfaceEnabled();

// Enables the OmniboxCrashFixKillSwitch feature.
BASE_DECLARE_FEATURE(kOmniboxCrashFixKillSwitch);

// Returns true if the OmniboxCrashFixKillSwitch feature is enabled.
bool IsOmniboxCrashFixKillSwitchEnabled();

// Enables the AIMEligibilityServiceStartWithProfile feature.
BASE_DECLARE_FEATURE(kAIMEligibilityServiceStartWithProfile);

// Returns true if the AIMEligibilityServiceStartWithProfile feature is enabled.
bool IsAIMEligibilityServiceStartWithProfileEnabled();

// Enables the AIMNTPEntrypointTablet feature.
BASE_DECLARE_FEATURE(kAIMNTPEntrypointTablet);

// Returns true if the AIMNTPEntrypointTablet feature is enabled.
bool IsAIMNTPEntrypointTabletEnabled();

// Enables the AIMEligibilityRefreshNTPModules feature.
BASE_DECLARE_FEATURE(kAIMEligibilityRefreshNTPModules);

// Returns true if the AIMEligibilityRefreshNTPModules feature is enabled.
bool IsAIMEligibilityRefreshNTPModulesEnabled();

// Enables the IOSWebContextMenuNewTitle feature.
BASE_DECLARE_FEATURE(kIOSWebContextMenuNewTitle);

// Returns true if the IOSWebContextMenuNewTitle feature is enabled.
bool IsIOSWebContextMenuNewTitleEnabled();

// Enables the CloseOtherTabs feature.
BASE_DECLARE_FEATURE(kCloseOtherTabs);

// Returns true if the CloseOtherTabs feature is enabled.
bool IsCloseOtherTabsEnabled();

// Feature flag to enable the Assistant Container.
BASE_DECLARE_FEATURE(kAssistantContainer);

// Returns true if the Assistant Container is enabled.
bool IsAssistantContainerEnabled();

// Enables the ComposeboxIpad feature.
BASE_DECLARE_FEATURE(kComposeboxIpad);

// Returns true if the ComposeboxIpad feature is enabled.
bool IsComposeboxIpadEnabled();

// Enables the ChromeNextIa feature.
BASE_DECLARE_FEATURE(kChromeNextIa);

// Returns true if the ChromeNextIa feature is enabled.
bool IsChromeNextIaEnabled();

// Enables the ComposeboxAIMDisabled feature.
BASE_DECLARE_FEATURE(kComposeboxAIMDisabled);

// Returns true if the ComposeboxAIMDisabled feature is enabled.
bool IsComposeboxAIMDisabled();

// Enables the EnableNewStartupFlow feature.
BASE_DECLARE_FEATURE(kEnableNewStartupFlow);

// Returns true if the EnableNewStartupFlow feature is enabled.
bool IsEnableNewStartupFlowEnabled();

// Updates EnableNewStartupFlow NSUserDefaults key if the value was changed.
void SaveEnableNewStartupFlowForNextStart();

// Resets the cached value for IsEnableNewStartupFlowEnabled, needed for tests.
void ResetEnableNewStartupFlowEnabledForTesting();

// Flags for Share Ablation study.
BASE_DECLARE_FEATURE(kDisableShareButton);
BASE_DECLARE_FEATURE(kShareInOmniboxLongPress);
BASE_DECLARE_FEATURE(kShareInOverflowMenu);
BASE_DECLARE_FEATURE(kShareInVerbatimMatch);

BASE_DECLARE_FEATURE(kUseSceneViewController);

// Returns true if the UseSceneViewController feature is enabled.
bool IsUseSceneViewControllerEnabled();

// Enables the DisableComposeboxFromAIMNTP feature.
BASE_DECLARE_FEATURE(kDisableComposeboxFromAIMNTP);

// Returns true if the DisableComposeboxFromAIMNTP feature is enabled.
bool IsDisableComposeboxFromAIMNTPEnabled();

// Enables the AIMCobrowseDebugEntrypoint feature.
BASE_DECLARE_FEATURE(kAIMCobrowseDebugEntrypoint);

// Returns true if the AIMCobrowseDebugEntrypoint feature is enabled.
bool IsAIMCobrowseDebugEntrypointEnabled();

// Enables recording the number of recent days with active sessions.
BASE_DECLARE_FEATURE(kRecordRecentActiveDays);

// Returns true if kRecordRecentActiveDays is enabled.
bool IsRecordRecentActiveDaysEnabled();

// Enables the AimCobrowse feature.
BASE_DECLARE_FEATURE(kAimCobrowse);

// Returns true if the AimCobrowse feature is enabled.
bool IsAimCobrowseEnabled();

// Enables the DisableU18FeedbackIos feature.
BASE_DECLARE_FEATURE(kDisableU18FeedbackIos);

// Returns true if the DisableU18FeedbackIos feature is enabled.
bool IsDisableU18FeedbackIosEnabled();

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_FEATURES_FEATURES_H_
