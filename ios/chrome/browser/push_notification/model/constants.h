// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import <string>

enum class PushNotificationClientId;

// Enum specifying the various types of push notifications. Entries should not
// be renumbered and numeric values should never be reused.
// LINT.IfChange(NotificationType)
enum class NotificationType {
  kTipsDefaultBrowser = 0,
  kTipsWhatsNew = 1,
  kTipsSignin = 2,
  kTipsSetUpListContinuation = 3,
  kTipsDocking = 4,
  kTipsOmniboxPosition = 5,
  kTipsLens = 6,
  kTipsEnhancedSafeBrowsing = 7,
  kSafetyCheckUpdateChrome = 8,
  kSafetyCheckPasswords = 9,
  kSafetyCheckSafeBrowsing = 10,
  kTipsLensOverlay = 11,
  kTipsCPE = 12,
  kTipsIncognitoLock = 13,
  kUnknown = 14,
  kSendTab = 15,
  kTipsTrustedVaultKeyRetrieval = 16,
  kReminder = 17,
  kCommerce = 18,
  kContent = 19,
  kMaxValue = kContent,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Enum for the metric logging the source of the native Notification enable
// alert. Entries should not be renumbered and numeric values should never be
// reused.
// LINT.IfChange(NotificationOptInAccessPoint)
enum class NotificationOptInAccessPoint {
  kTips = 1,
  kSetUpList = 2,
  kSendTabMagicStackPromo = 3,
  kSafetyCheck = 4,
  kFeed = 5,
  kSettings = 6,
  kMaxValue = kSettings,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PushNotificationClientManagerFailurePoint)
enum class PushNotificationClientManagerFailurePoint {
  // Failed to get Profile-based `PushNotificationClientManager` when handling
  // foreground presentation (in
  // `-userNotificationCenter:willPresentNotification:withCompletionHandler:`).
  kWillPresentNotification = 0,
  // Failed to get Profile-based `PushNotificationClientManager` during APNS
  // registration (in `-applicationDidRegisterWithAPNS:profile:`).
  kDidRegisterWithAPNS = 1,
  // Failed to get Profile-based `PushNotificationClientManager` when the app
  // entered foreground (in `-appDidEnterForeground:`).
  kAppDidEnterForeground = 2,
  // DEPRECATED: This failure point is no longer used as the code path
  // that logged it has been removed.
  // kHandleNotificationResponse = 3,
  // Failed to get Profile-based `PushNotificationClientManager` when processing
  // an incoming remote notification in the background (in
  // `-applicationWillProcessIncomingRemoteNotification:`).
  kWillProcessIncomingRemoteNotification = 4,
  // Failed inside `GetClientManagerForProfile()` because the input
  // `ProfileIOS*` was `nullptr`.
  kGetClientManagerNullProfileInput = 5,
  // Failed inside `GetClientManagerForProfile()` because the
  // `PushNotificationProfileService` couldn't be retrieved.
  kGetClientManagerMissingProfileService = 6,
  // Failed inside `GetClientManagerForUserInfo()` because the Profile name
  // could not be retrieved from user info.
  kGetClientManagerFailedToGetProfileName = 7,
  // Failed inside `GetClientManagerForUserInfo()` because the Profile couldn't
  // be found (or wasn't loaded) using the name from user info.
  kGetClientManagerProfileNotFoundByName = 8,
  // Failed inside `GetProfileNameFromUserInfo()` because the
  // profile name was missing/empty in user info.
  kGetProfileNameEmptyNameProvided = 9,
  // Failed inside `GetProfileNameFromUserInfo()` because the
  // profile name was not found in storage.
  kGetProfileNameDirectNameNotFoundInStorage = 10,
  // Failed inside `HandleNotificationInteractionAfterProfileSwitch()` because
  // the client manager couldn't be retrieved for the switched profile.
  kInteractionContinuationMissingClientManager = 11,
  // Failed inside `-handleProfileSpecificNotificationResponse:` because profile
  // name extraction/validation failed overall (returned empty `profileName`).
  kHandleInteractionInvalidProfileName = 12,
  // Recorded inside `-handleProfileSpecificNotificationResponse:` when the
  // notification's target scene couldn't be found via
  // `-notificationTargetSceneStateForResponse:`. The system will attempt to
  // fall back to the foreground active scene. Note: Unlike other entries, this
  // indicates a fallback mechanism being used, not an outright failure that
  // prevents further processing. Logged for monitoring purposes.
  kHandleInteractionMissingTargetScene = 13,
  // Failed inside `-handleProfileSpecificNotificationResponse:` because the
  // fallback foreground scene (`self.foregroundActiveScene`) was also missing.
  kHandleInteractionMissingFallbackScene = 14,
  // Failed inside `-notificationTargetSceneStateForResponse:` because
  // `response.targetScene` was `nil`.
  kGetResponseTargetSceneNil = 15,
  // Failed inside `GetProfileNameFromUserInfo()` because the string provided
  // for `kOriginatingGaiaIDKey` was either missing or empty.
  kGetProfileNameMissingOrEmptyGaiaID = 16,
  // Failed inside `GetProfileNameFromUserInfo()` because the valid Gaia ID
  // extracted from `kOriginatingGaiaIDKey` could not be mapped to any known
  // Profile name via `AccountProfileMapper`.
  kGetProfileNameGaiaIdNotMapped = 17,
  // Failed inside `GetProfileNameFromUserInfo()` because the profile name,
  // successfully obtained by mapping a Gaia ID, was not found in
  // `ProfileAttributesStorageIOS` (e.g., stale mapping).
  kGetProfileNameMappedNameNotFoundInStorage = 18,
  // Failed inside `GetClientManagerForUserInfo()` because the attempt to load
  // an existing (but unloaded) Profile via `LoadProfileAsync()` failed
  // (the completion callback received `nullptr`).
  kGetClientManagerProfileLoadFailed = 19,
  kMaxValue = kGetClientManagerProfileLoadFailed,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:PushNotificationClientManagerFailurePoint)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Used in the histogram
// IOS.PushNotification.ProfileRequestCreationFailureReason.{ClientId}.
//
// LINT.IfChange(ProfileNotificationRequestCreationFailureReason)
enum class ProfileNotificationRequestCreationFailureReason {
  // Failed inside CreateRequestForProfile() because the input Profile name was
  // empty. Considered an invalid input state.
  kInvalidProfileName = 0,
  // Failed inside CreateRequestForProfile() because the request.time_interval
  // (base::TimeDelta) in the input ScheduledNotificationRequest was not
  // positive (i.e., it was zero or negative). This is an invalid state for
  // UNTimeIntervalNotificationTrigger.
  kInvalidTimeInterval = 1,
  // Failed inside CreateRequestForProfile() because the request.identifier (an
  // NSString*) in the input ScheduledNotificationRequest was
  // nil or an empty string. Both are considered invalid states.
  kInvalidIdentifier = 2,
  // Failed inside CreateRequestForProfile() because the request.content (a
  // UNNotificationContent*) in the input ScheduledNotificationRequest was nil.
  // Considered an invalid input state.
  kInvalidSourceContent = 3,
  // Failed inside CreateRequestForProfile() because calling [mutableCopy] on
  // the (non-nil) request.content returned nil. This typically indicates a
  // failure during memory allocation.
  kContentCopyFailed = 4,
  // Failed inside CreateRequestForProfile() because creating the
  // UNTimeIntervalNotificationTrigger by calling
  // `triggerWithTimeInterval` returned nil.
  kTriggerCreationFailed = 5,
  kMaxValue = kTriggerCreationFailed,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:ProfileNotificationRequestCreationFailureReason)

// LINT.IfChange(PushNotificationTargetProfileHandlingResult)
enum class PushNotificationTargetProfileHandlingResult {
  // Success: The notification's target Profile was already the active Profile
  // in the target UI scene when the interaction was received. No profile switch
  // was needed.
  kCorrectProfileActive = 0,
  // Success: The notification's target Profile was *not* the active Profile in
  // the target UI scene. A profile switch was successfully initiated to ensure
  // the correct context handles the interaction.
  kSwitchEnsuredCorrectProfile = 1,
  // The originating Profile for the notification could not be determined from
  // the notification's metadata (e.g., missing or invalid profile name/Gaia
  // ID).
  kProfileUnidentifiable = 2,
  // Failure: No suitable UI scene (neither the specific target scene nor a
  // foreground fallback) could be found to handle the notification interaction.
  kFailureSceneUnavailable = 3,
  kMaxValue = kFailureSceneUnavailable,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:PushNotificationTargetProfileHandlingResult)

// Enum for the NAU implementation for Content notifications. Change
// NotificationActionType enum when this one changes.
typedef NS_ENUM(NSInteger, NAUActionType) {
  // When a content notification is displayed on the device.
  NAUActionTypeDisplayed = 0,
  // When a content notification is opened.
  NAUActionTypeOpened = 1,
  // When a content notification is dismissed.
  NAUActionTypeDismissed = 2,
  // When the feedback secondary action is triggered.
  NAUActionTypeFeedbackClicked = 3,
};

// Enum for the NAU implementation for Content notifications used for metrics.
enum class NotificationActionType {
  // When a content notification is displayed on the device.
  kNotificationActionTypeDisplayed = NAUActionTypeDisplayed,
  // When a content notification is opened.
  kNotificationActionTypeOpened = NAUActionTypeOpened,
  // When a content notification is dismissed.
  kNotificationActionTypeDismissed = NAUActionTypeDismissed,
  // When the feedback secondary action is triggered.
  kNotificationActionTypeFeedbackClicked = NAUActionTypeFeedbackClicked,
  // Max value.
  kMaxValue = kNotificationActionTypeFeedbackClicked,
};

// Enum for the NAU implementation for Content notifications.
typedef NS_ENUM(NSInteger, SettingsToggleType) {
  // None of the toggles has changed.
  SettingsToggleTypeNone = 0,
  // The settings toggle identifier for Content for NAU.
  SettingsToggleTypeContent = 1,
  // The settings toggle identifier for sports for NAU.
  SettingsToggleTypeSports = 2,
};

// Key of commerce notification used in pref
// `kFeaturePushNotificationPermissions`.
extern const char kCommerceNotificationKey[];

// Key of content notification used in pref
// `kFeaturePushNotificationPermissions`.
extern const char kContentNotificationKey[];

// Key of sports notification used in pref
// `kFeaturePushNotificationPermissions`.
extern const char kSportsNotificationKey[];

// Key of tips notification used in pref `kFeaturePushNotificationPermissions`.
extern const char kTipsNotificationKey[];

// Key of send tab notification used in pref
// `kFeaturePushNotificationPermissions`.
extern const char kSendTabNotificationKey[];

// Key of Safety Check notification used in pref
// `kFeaturePushNotificationPermissions`.
extern const char kSafetyCheckNotificationKey[];

// Key of Reminder notification used in pref
// `kFeaturePushNotificationPermissions`.
extern const char kReminderNotificationKey[];

// Key of Cross-Platform Promos notification used in pref
// `kFeaturePushNotificationPermissions`.
extern const char kCrossPlatformPromosNotificationKey[];
// Action identifier for the Content Notifications Feedback action.
extern NSString* const kContentNotificationFeedbackActionIdentifier;

// Category identifier for the Content Notifications category that contains a
// Feedback action.
extern NSString* const kContentNotificationFeedbackCategoryIdentifier;

// The body parameter of the notification for a Content Notification delivered
// NAU.
extern NSString* const kContentNotificationNAUBodyParameter;

// The NSUserDefaults key for the delivered content notifications that need to
// be reported.
extern NSString* const kContentNotificationContentArrayKey;

// The histogram name for the NAU success metric.
extern const char kNAUHistogramName[];

// The histogram name to record a Content Notification action.
extern const char kContentNotificationActionHistogramName[];

// The max amount of NAU sends per session.
extern const int kDeliveredNAUMaxSendsPerSession;

// Key for the Push Notification Client Id type in notification payload. Used
// for Send Tab notifications.
extern NSString* const kPushNotificationClientIdKey;

// Key used in UNNotificationContent.userInfo to store the Profile name that
// originated the local notification. Used for mapping local notifications to
// the correct Profile on the device.
extern NSString* const kOriginatingProfileNameKey;

// Key used in UNNotificationContent.userInfo to store the obfuscated Gaia ID
// that originated the remote notification. Used for mapping remote
// notifications to the correct Profile on the device.
extern NSString* const kOriginatingGaiaIDKey;

// Returns the string representation of the given `client_id`. This string is
// used to store the client's push notification permission settings in the pref
// service, as a preference key on the push notification server, and for
// suffixing client-specific UMA histogram names (e.g.,
// `IOS.PushNotification.ProfileRequestCreationFailureReason.{ClientId}`).
std::string PushNotificationClientIdToString(
    PushNotificationClientId client_id);

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_CONSTANTS_H_
