// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_CONSTANTS_H_
#define IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Constants that are shared between apps belonging to the chrome iOS app group.
// They are mainly used for communication between applications in the group.
namespace app_group {

// An enum of the different application member of the Chrome app group.
// To ensure continuity in metrics log, applications can only be added at the
// end.
// Applications directly sending metrics must be added to this enum.
enum AppGroupApplications {
  APP_GROUP_CHROME = 0,
  APP_GROUP_TODAY_EXTENSION,
};

// The different types of outcome used for UMA and created by the open
// extension.
// The entries should not be removed or reordered.
// Also add the name of the enum and histogram.
enum class OpenExtensionOutcome : NSInteger {
  kSuccess = 0,
  kInvalid = 1,
  kFailureInvalidURL = 2,
  kFailureURLNotFound = 3,
  kFailureOpenInNotFound = 4,
  kFailureUnsupportedScheme = 5,
  kMaxValue = kFailureUnsupportedScheme,
};

// The different types of item that can be created by the share extension.
enum ShareExtensionItemType {
  READING_LIST_ITEM = 0,
  BOOKMARK_ITEM,
  OPEN_IN_CHROME_ITEM
};

// The key of a preference containing a dictionary of capabilities supported by
// the current version of Chrome.
extern NSString* const kChromeCapabilitiesPreference;

// ---- Chrome capabilities -----
// Show default browser promo capability.
extern NSString* const kChromeShowDefaultBrowserPromoCapability;

// The x-callback-url indicating that an application in the group requires a
// command.
extern const char kChromeAppGroupXCallbackCommand[];

// The key of a preference containing a dictionary of field trial values needed
// in extensions.
extern NSString* const kChromeExtensionFieldTrialPreference;

// The key of a preference containing a dictionary containing app group command
// parameters.
extern const char kChromeAppGroupCommandPreference[];

// The key in kChromeAppGroupCommandPreference containing the ID of the
// application requesting a x-callback-url command.
extern const char kChromeAppGroupCommandAppPreference[];

// The key in kChromeAppGroupCommandPreference containing the command requested
// by `kChromeAppGroupCommandAppPreference`.
extern const char kChromeAppGroupCommandCommandPreference[];

// The command to open a URL. Parameter must contain the URL.
extern const char kChromeAppGroupOpenURLCommand[];

// The command to search some text. Parameter must contain the text.
extern const char kChromeAppGroupSearchTextCommand[];

// The command to search an image. Data parameter must contain the image.
extern const char kChromeAppGroupSearchImageCommand[];

// The command to trigger a voice search.
extern const char kChromeAppGroupVoiceSearchCommand[];

// The command to open a new tab.
extern const char kChromeAppGroupNewTabCommand[];

// The command to focus the omnibox.
extern const char kChromeAppGroupFocusOmniboxCommand[];

// The command to open an incognito search.
extern const char kChromeAppGroupIncognitoSearchCommand[];

// The command to open the QR Code scanner.
extern const char kChromeAppGroupQRScannerCommand[];

// The command to open Lens.
extern const char kChromeAppGroupLensCommand[];

// The command to open the Password Manager's search page.
extern const char kChromeAppGroupSearchPasswordsCommand[];

// The key in kChromeAppGroupCommandPreference containing a NSDate at which
// `kChromeAppGroupCommandAppPreference` issued the command.
extern const char kChromeAppGroupCommandTimePreference[];

// The key in kChromeAppGroupCommandPreference containing the text use for the
// command if it requires one. This could be a URL, a string, etc.
extern const char kChromeAppGroupCommandTextPreference[];

// The key in kChromeAppGroupCommandPreference containing the data to use for
// the command if it requires one. This could be an image, etc.
extern const char kChromeAppGroupCommandDataPreference[];

// The key in kChromeAppGroupCommandPreference containing the index to open for
// if the command requires one.
extern const char kChromeAppGroupCommandIndexPreference[];

// The key of a preference containing whether the current default search engine
// supports Search by Image.
extern const char kChromeAppGroupSupportsSearchByImage[];

// The key of a preference containing whether Google is the default search
// engine.
extern const char kChromeAppGroupIsGoogleDefaultSearchEngine[];

// The key of a preference containing whether the home screen widget should show
// a shortcut to Lens instead of the QR scanner if Google is the default search
// provider.
extern const char kChromeAppGroupEnableLensInWidget[];

// The key of a preference containing whether the home screen widget should show
// the color Lens and voice icons if Lens is shown.
extern const char kChromeAppGroupEnableColorLensAndVoiceIconsInWidget[];

// The key of a preference containing Chrome client ID reported in the metrics
// client ID. If the user does not opt in, this value must be cleared from the
// shared user defaults.
extern const char kChromeAppClientID[];

// The key of a preference containing the timestamp when the user enabled
// metrics reporting.
extern const char kUserMetricsEnabledDate[];

// The six keys of the items sent by the share extension to Chrome (source, URL,
// title, date, cancel, type).
extern NSString* const kShareItemSource;
extern NSString* const kShareItemURL;
extern NSString* const kShareItemTitle;
extern NSString* const kShareItemDate;
extern NSString* const kShareItemCancel;
extern NSString* const kShareItemType;

// The value used by Chrome Share extension in `kShareItemSource`.
extern NSString* const kShareItemSourceShareExtension;

// The values used by Chrome extensions in
// `kChromeAppGroupCommandAppPreference`.
extern NSString* const kOpenCommandSourceTodayExtension;
extern NSString* const kOpenCommandSourceContentExtension;
extern NSString* const kOpenCommandSourceSearchExtension;
extern NSString* const kOpenCommandSourceShareExtension;
extern NSString* const kOpenCommandSourceCredentialsExtension;
extern NSString* const kOpenCommandSourceOpenExtension;

// The value of the key for the sharedDefaults used by the Content Widget.
extern NSString* const kSuggestedItems;

// The value of the key for the sharedDefaults last modification date used by
// the Shortcuts Widget.
extern NSString* const kSuggestedItemsLastModificationDate;

// The current epoch time, on the first run of chrome on this machine. It is set
// once and must be attached to metrics reports forever thereafter.
extern const char kInstallDate[];

// The brand code string associated with the install. This brand code will be
// added to metrics logs.
extern const char kBrandCode[];

// The five keys of the outcomes by the open extension to Chrome (Success,
// FailureInvalidURL, FailureURLNotFound, FailureOpenInNotFound,
// FailureUnsupportedScheme).
extern NSString* const kOpenExtensionOutcomeSuccess;
extern NSString* const kOpenExtensionOutcomeFailureInvalidURL;
extern NSString* const kOpenExtensionOutcomeFailureURLNotFound;
extern NSString* const kOpenExtensionOutcomeFailureOpenInNotFound;
extern NSString* const kOpenExtensionOutcomeFailureUnsupportedScheme;

// A key in the application group NSUserDefault that contains
// the outcomes of the Open Extension.
extern NSString* const kOpenExtensionOutcomes;

// Conversion helpers between keys and OpenExtensionOutcome.
NSString* KeyForOpenExtensionOutcomeType(OpenExtensionOutcome);
OpenExtensionOutcome OutcomeTypeFromKey(NSString*);

// Gets the application group.
NSString* ApplicationGroup();

// Gets the common application group.
NSString* CommonApplicationGroup();

// Gets the legacy share extension folder URL.
// This folder is deprecated and will be removed soon. Please do not add items
// to it.
// TODO(crbug.com/41303853): Remove this value.
NSURL* LegacyShareExtensionItemsFolder();

// Gets the shared folder URL containing commands from other applications.
NSURL* ExternalCommandsItemsFolder();

// Gets the shared folder URL in which favicons used by the content widget are
// stored.
NSURL* ContentWidgetFaviconsFolder();

// Gets the shared folder URL in which favicon attributes used by the credential
// provider extensions are stored.
NSURL* SharedFaviconAttributesFolder();

// Gets the shared folder URL in which Crashpad reports are stored.
NSURL* CrashpadFolder();

// Returns an autoreleased pointer to the shared user defaults if an
// application group is defined for the application and its extensions.
// If not (i.e. on simulator, or if entitlements do not allow it) returns
// [NSUserDefaults standardUserDefaults].
NSUserDefaults* GetGroupUserDefaults();

// Returns an autoreleased pointer to the shared user defaults if a group is
// defined for the application and other application of the same developer. If
// not (i.e. on simulator, or if entitlements do not allow it) returns
// [NSUserDefaults standardUserDefaults].
NSUserDefaults* GetCommonGroupUserDefaults();

// The application name of `application`.
NSString* ApplicationName(AppGroupApplications application);

}  // namespace app_group

#endif  // IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_CONSTANTS_H_
