// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_CONSTANTS_H_
#define EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_CONSTANTS_H_

namespace extension_management_api_constants {

// Keys used for incoming arguments and outgoing JSON data.
extern const char kDisabledReasonKey[];

// Values for outgoing JSON data.
extern const char kDisabledReasonPermissionsIncrease[];

// Error messages.
extern const char kExtensionCreateError[];
extern const char kGestureNeededForEscalationError[];
extern const char kGestureNeededForMV2DeprecationReEnableError[];
extern const char kGestureNeededForUninstallError[];
extern const char kManifestParseError[];
extern const char kNoExtensionError[];
extern const char kNotAnAppError[];
extern const char kUserCantModifyError[];
extern const char kUninstallCanceledError[];
extern const char kUserDidNotReEnableError[];
extern const char kMissingRequirementsError[];
extern const char kGestureNeededForCreateAppShortcutError[];
extern const char kNoBrowserToCreateShortcut[];
extern const char kCreateOnlyPackagedAppShortcutMac[];
extern const char kCreateShortcutCanceledError[];
extern const char kGestureNeededForSetLaunchTypeError[];
extern const char kLaunchTypeNotAvailableError[];
extern const char kGestureNeededForGenerateAppForLinkError[];
extern const char kInvalidURLError[];
extern const char kEmptyTitleError[];
extern const char kGenerateAppForLinkInstallError[];
extern const char kNotAllowedInKioskError[];
extern const char kCannotChangePrimaryKioskAppError[];
extern const char kInstallReplacementWebAppInvalidWebAppError[];
extern const char kInstallReplacementWebAppInvalidContextError[];
extern const char kInstallReplacementWebAppNotFromWebstoreError[];
extern const char kGestureNeededForInstallReplacementWebAppError[];
extern const char kParentPermissionFailedError[];
extern const char kChromeAppsDeprecated[];

}  // namespace extension_management_api_constants

#endif  // EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_CONSTANTS_H_
