// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_CONSTANTS_H_
#define EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_CONSTANTS_H_

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extension_management_api_constants {

// Keys used for incoming arguments and outgoing JSON data.
inline constexpr char kDisabledReasonKey[] = "disabledReason";

// Values for outgoing JSON data.
inline constexpr char kDisabledReasonPermissionsIncrease[] =
    "permissions_increase";

// Error messages.
inline constexpr char kExtensionCreateError[] =
    "Failed to create extension from manifest.";
inline constexpr char kGestureNeededForEscalationError[] =
    "Re-enabling an extension disabled due to permissions increase "
    "requires a user gesture.";
inline constexpr char kGestureNeededForMV2DeprecationReEnableError[] =
    "Re-enabling an extension disabled due to MV2 deprecation requires a user "
    "gesture.";
inline constexpr char kGestureNeededForUninstallError[] =
    "chrome.management.uninstall requires a user gesture.";
inline constexpr char kManifestParseError[] = "Failed to parse manifest.";
inline constexpr char kNoExtensionError[] =
    "Failed to find extension with id *.";
inline constexpr char kNotAnAppError[] = "Extension * is not an App.";
inline constexpr char kUserCantModifyError[] =
    "Extension * cannot be modified by user.";
inline constexpr char kUninstallCanceledError[] =
    "Extension * uninstall canceled by user.";
inline constexpr char kUserDidNotReEnableError[] =
    "The user did not accept the re-enable dialog.";
inline constexpr char kMissingRequirementsError[] =
    "There were missing requirements: *";
inline constexpr char kGestureNeededForCreateAppShortcutError[] =
    "chrome.management.createAppShortcut requires a user gesture.";
inline constexpr char kNoBrowserToCreateShortcut[] =
    "There is no browser window to create shortcut.";
inline constexpr char kCreateOnlyPackagedAppShortcutMac[] =
    "Shortcuts can only be created for new-style packaged apps on Mac.";
inline constexpr char kCreateShortcutCanceledError[] =
    "App shortcuts creation canceled by user.";
inline constexpr char kGestureNeededForSetLaunchTypeError[] =
    "chrome.management.setLaunchType requires a user gesture.";
inline constexpr char kLaunchTypeNotAvailableError[] =
    "The launch type is not available for this app.";
inline constexpr char kGestureNeededForGenerateAppForLinkError[] =
    "chrome.management.generateAppForLink requires a user gesture.";
inline constexpr char kInvalidURLError[] = "The URL \"*\" is invalid.";
inline constexpr char kEmptyTitleError[] = "The title can not be empty.";
inline constexpr char kGenerateAppForLinkInstallError[] =
    "Failed to install the generated app.";
inline constexpr char kNotAllowedInKioskError[] = "Not allowed in kiosk.";
inline constexpr char kCannotChangePrimaryKioskAppError[] =
    "Cannot change the primary kiosk app state.";
inline constexpr char kInstallReplacementWebAppInvalidWebAppError[] =
    "Web app is not a valid installable web app.";
inline constexpr char kInstallReplacementWebAppInvalidContextError[] =
    "Web apps can't be installed in the current user profile.";
inline constexpr char kInstallReplacementWebAppNotFromWebstoreError[] =
    "Only extensions from the web store can install replacement web apps.";
inline constexpr char kGestureNeededForInstallReplacementWebAppError[] =
    "chrome.management.installReplacementWebApp requires a user gesture.";
inline constexpr char kParentPermissionFailedError[] =
    "Parent Permission Request Failed.";
inline constexpr char kChromeAppsDeprecated[] =
    "Chrome app * is deprecated on Window, Mac, and Linux. "
    "See https://support.google.com/chrome/?p=chrome_app_deprecation for more "
    "info";

// Unsupported APIs.
// TODO(crbug.com/371332103): Add platform name.
inline constexpr char kLaunchAppNotSupported[] =
    "chrome.management.launchApp is not supported on this platform.";
inline constexpr char kCreateAppShortcutNotSupported[] =
    "chrome.management.createAppShortcut is not supported on this platform.";
inline constexpr char kGenerateAppForLinkNotSupported[] =
    "chrome.management.generateAppForLink is not supported on this platform.";

}  // namespace extension_management_api_constants

#endif  // EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_CONSTANTS_H_
