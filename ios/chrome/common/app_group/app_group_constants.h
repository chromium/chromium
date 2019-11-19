// Copyright 2015 The Chromium Authors. All rights reserved.
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

// The different types of item that can be created by the share extension.
enum ShareExtensionItemType {
  READING_LIST_ITEM = 0,
  BOOKMARK_ITEM,
  OPEN_IN_CHROME_ITEM
};

// The x-callback-url indicating that an application in the group requires a
// command.
extern const char kChromeAppGroupXCallbackCommand[];

// The key of a preference containing a dictionary of field trial values needed
// in extensions.
extern const char kChromeExtensionFieldTrialPreference[];

// The key of a preference containing a dictionary containing app group command
// parameters.
extern const char kChromeAppGroupCommandPreference[];

// The key in kChromeAppGroupCommandPreference containing the ID of the
// application requesting a x-callback-url command.
extern const char kChromeAppGroupCommandAppPreference[];

// The key in kChromeAppGroupCommandPreference containing the command requested
// by |kChromeAppGroupCommandAppPreference|.
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

// The key in kChromeAppGroupCommandPreference containing a NSDate at which
// |kChromeAppGroupCommandAppPreference| issued the command.
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

// The value used by Chrome Share extension in |kShareItemSource|.
extern NSString* const kShareItemSourceShareExtension;

// The values used by Chrome extensions in
// |kChromeAppGroupCommandAppPreference|.
extern NSString* const kOpenCommandSourceTodayExtension;
extern NSString* const kOpenCommandSourceContentExtension;
extern NSString* const kOpenCommandSourceSearchExtension;
extern NSString* const kOpenCommandSourceShareExtension;

// The value of the key for the sharedDefaults used by the Content Widget.
extern NSString* const kSuggestedItems;

// The current epoch time, on the first run of chrome on this machine. It is set
// once and must be attached to metrics reports forever thereafter.
extern const char kInstallDate[];

// The brand code string associated with the install. This brand code will be
// added to metrics logs.
extern const char kBrandCode[];

// Gets the application group.
NSString* ApplicationGroup();

// Gets the common application group.
NSString* CommonApplicationGroup();

// Gets the legacy share extension folder URL.
// This folder is deprecated and will be removed soon. Please do not add items
// to it.
// TODO(crbug.com/695381): Remove this value.
NSURL* LegacyShareExtensionItemsFolder();

// Gets the shared folder URL containing commands from other applications.
NSURL* ExternalCommandsItemsFolder();

// Gets the shared folder URL in which favicons used by the content widget are
// stored.
NSURL* ContentWidgetFaviconsFolder();

// Returns an autoreleased pointer to the shared user defaults if an
// application group is defined. If not (i.e. on simulator, or if entitlements
// do not allow it) returns [NSUserDefaults standardUserDefaults].
NSUserDefaults* GetGroupUserDefaults();

// The application name of |application|.
NSString* ApplicationName(AppGroupApplications application);

}  // namespace app_group

#endif  // IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_CONSTANTS_H_
