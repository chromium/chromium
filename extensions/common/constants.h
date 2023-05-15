// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_CONSTANTS_H_
#define EXTENSIONS_COMMON_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

#include "base/files/file_path.h"
#include "base/strings/string_piece_forward.h"
#include "build/chromeos_buildflags.h"
#include "extensions/common/extensions_export.h"

namespace extensions {

// Scheme we serve extension content from.
EXTENSIONS_EXPORT extern const char kExtensionScheme[];

// The name of the manifest inside an extension.
EXTENSIONS_EXPORT extern const base::FilePath::CharType kManifestFilename[];

// The name of the differential fingerprint file inside an extension.
EXTENSIONS_EXPORT extern const base::FilePath::CharType
    kDifferentialFingerprintFilename[];

// The name of locale folder inside an extension.
EXTENSIONS_EXPORT extern const base::FilePath::CharType kLocaleFolder[];

// The name of the messages file inside an extension.
EXTENSIONS_EXPORT extern const base::FilePath::CharType kMessagesFilename[];

// The name of the gzipped messages file inside an extension.
EXTENSIONS_EXPORT extern const base::FilePath::CharType
    kGzippedMessagesFilename[];

// The base directory for subdirectories with platform-specific code.
EXTENSIONS_EXPORT extern const base::FilePath::CharType
    kPlatformSpecificFolder[];

// A directory reserved for metadata, generated either by the webstore
// or chrome.
EXTENSIONS_EXPORT extern const base::FilePath::CharType kMetadataFolder[];

// Name of the verified contents file within the metadata folder.
EXTENSIONS_EXPORT extern const base::FilePath::CharType
    kVerifiedContentsFilename[];

// Name of the computed hashes file within the metadata folder.
EXTENSIONS_EXPORT extern const base::FilePath::CharType
    kComputedHashesFilename[];

// Name of the indexed ruleset directory for the Declarative Net Request API.
EXTENSIONS_EXPORT extern const base::FilePath::CharType
    kIndexedRulesetDirectory[];

// The name of the directory inside the profile where extensions are
// installed to.
EXTENSIONS_EXPORT extern const char kInstallDirectoryName[];

// The name of the directory inside the profile where unpacked (e.g. from .zip
// file) extensions are installed to.
EXTENSIONS_EXPORT extern const char kUnpackedInstallDirectoryName[];

// The name of a temporary directory to install an extension into for
// validation before finalizing install.
EXTENSIONS_EXPORT extern const char kTempExtensionName[];

// The file to write our decoded message catalogs to, relative to the
// extension_path.
EXTENSIONS_EXPORT extern const char kDecodedMessageCatalogsFilename[];

// The filename to use for a background page generated from
// background.scripts.
EXTENSIONS_EXPORT extern const char kGeneratedBackgroundPageFilename[];

// The URL piece between the extension ID and favicon URL.
EXTENSIONS_EXPORT extern const char kFaviconSourcePath[];

// Path to imported modules.
EXTENSIONS_EXPORT extern const char kModulesDir[];

// The file extension (.crx) for extensions.
EXTENSIONS_EXPORT extern const base::FilePath::CharType
    kExtensionFileExtension[];

// The file extension (.pem) for private key files.
EXTENSIONS_EXPORT extern const base::FilePath::CharType
    kExtensionKeyFileExtension[];

// Default frequency for auto updates, if turned on.
EXTENSIONS_EXPORT extern const int kDefaultUpdateFrequencySeconds;

// The name of the directory inside the profile where per-app local settings
// are stored.
EXTENSIONS_EXPORT extern const base::FilePath::CharType
    kLocalAppSettingsDirectoryName[];

// The name of the directory inside the profile where per-extension local
// settings are stored.
EXTENSIONS_EXPORT extern const base::FilePath::CharType
    kLocalExtensionSettingsDirectoryName[];

// The name of the directory inside the profile where per-app synced settings
// are stored.
EXTENSIONS_EXPORT extern const base::FilePath::CharType
    kSyncAppSettingsDirectoryName[];

// The name of the directory inside the profile where per-extension synced
// settings are stored.
EXTENSIONS_EXPORT extern const base::FilePath::CharType
    kSyncExtensionSettingsDirectoryName[];

// The name of the directory inside the profile where per-extension persistent
// managed settings are stored.
EXTENSIONS_EXPORT extern const base::FilePath::CharType
    kManagedSettingsDirectoryName[];

// The name of the database inside the profile where chrome-internal
// extension state resides.
EXTENSIONS_EXPORT extern const base::FilePath::CharType kStateStoreName[];

// The name of the database inside the profile where declarative extension
// rules are stored.
EXTENSIONS_EXPORT extern const base::FilePath::CharType kRulesStoreName[];

// The name of the database inside the profile where persistent dynamic user
// script metadata is stored.
EXTENSIONS_EXPORT extern const base::FilePath::CharType kScriptsStoreName[];

// Statistics are logged to UMA with these strings as part of histogram name.
// They can all be found under Extensions.Database.Open.<client>. Changing this
// needs to synchronize with histograms.xml, AND will also become incompatible
// with older browsers still reporting the previous values.
EXTENSIONS_EXPORT extern const char kSettingsDatabaseUMAClientName[];
EXTENSIONS_EXPORT extern const char kRulesDatabaseUMAClientName[];
EXTENSIONS_EXPORT extern const char kStateDatabaseUMAClientName[];
EXTENSIONS_EXPORT extern const char kScriptsDatabaseUMAClientName[];

// The URL query parameter key corresponding to multi-login user index.
EXTENSIONS_EXPORT extern const char kAuthUserQueryKey[];

// Mime type strings
EXTENSIONS_EXPORT extern const char kMimeTypeJpeg[];
EXTENSIONS_EXPORT extern const char kMimeTypePng[];

// The extension id of the Web Store component application.
EXTENSIONS_EXPORT extern const char kWebStoreAppId[];

// The key used for signing some pieces of data from the webstore.
EXTENSIONS_EXPORT extern const uint8_t kWebstoreSignaturesPublicKey[];
EXTENSIONS_EXPORT extern const size_t kWebstoreSignaturesPublicKeySize;

// A preference for storing the extension's update URL data.
EXTENSIONS_EXPORT extern const char kUpdateURLData[];

// Thread identifier for the main renderer thread (as opposed to a service
// worker thread).
// This is the default thread id used for extension event listeners registered
// from a non-service worker context
EXTENSIONS_EXPORT extern const int kMainThreadId;

// Enumeration of possible app launch sources.
// This should be kept in sync with LaunchSource in
// extensions/common/api/app_runtime.idl, and GetLaunchSourceEnum() in
// extensions/browser/api/app_runtime/app_runtime_api.cc.
// Note the enumeration is used in UMA histogram so entries
// should not be re-ordered or removed.
enum class AppLaunchSource {
  kSourceNone = 0,
  kSourceUntracked = 1,
  kSourceAppLauncher = 2,
  kSourceNewTabPage = 3,
  kSourceReload = 4,
  kSourceRestart = 5,
  kSourceLoadAndLaunch = 6,
  kSourceCommandLine = 7,
  kSourceFileHandler = 8,
  kSourceUrlHandler = 9,
  kSourceSystemTray = 10,
  kSourceAboutPage = 11,
  kSourceKeyboard = 12,
  kSourceExtensionsPage = 13,
  kSourceManagementApi = 14,
  kSourceEphemeralAppDeprecated = 15,
  kSourceBackground = 16,
  kSourceKiosk = 17,
  kSourceChromeInternal = 18,
  kSourceTest = 19,
  kSourceInstalledNotification = 20,
  kSourceContextMenu = 21,
  kSourceArc = 22,
  kSourceIntentUrl = 23,        // App launch triggered by a URL.
  kSourceRunOnOsLogin = 24,     // App launched during OS login.
  kSourceProtocolHandler = 25,  // App launch via protocol handler.
  kSourceReparenting = 26,      // APP launch via reparenting.
  kSourceAppHomePage = 27,      // App launch from chrome://apps (App Home).

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kSourceAppHomePage,
};

// This enum is used for the launch type the user wants to use for an
// application.
// Do not remove items or re-order this enum as it is used in preferences
// and histograms.
enum LaunchType {
  LAUNCH_TYPE_INVALID = -1,
  LAUNCH_TYPE_FIRST = 0,
  LAUNCH_TYPE_PINNED = LAUNCH_TYPE_FIRST,
  LAUNCH_TYPE_REGULAR = 1,
  LAUNCH_TYPE_FULLSCREEN = 2,
  LAUNCH_TYPE_WINDOW = 3,
  NUM_LAUNCH_TYPES,

  // Launch an app in the in the way a click on the NTP would,
  // if no user pref were set.  Update this constant to change
  // the default for the NTP and chrome.management.launchApp().
  LAUNCH_TYPE_DEFAULT = LAUNCH_TYPE_REGULAR
};

}  // namespace extensions

namespace extension_misc {

// Matches chrome.tabs.TAB_ID_NONE.
EXTENSIONS_EXPORT extern const int kUnknownTabId;

// Matches chrome.windows.WINDOW_ID_NONE.
EXTENSIONS_EXPORT extern const int kUnknownWindowId;

// Matches chrome.windows.WINDOW_ID_CURRENT.
EXTENSIONS_EXPORT extern const int kCurrentWindowId;

using ExtensionIcons = int;
constexpr ExtensionIcons EXTENSION_ICON_GIGANTOR = 512;
constexpr ExtensionIcons EXTENSION_ICON_EXTRA_LARGE = 256;
constexpr ExtensionIcons EXTENSION_ICON_LARGE = 128;
constexpr ExtensionIcons EXTENSION_ICON_MEDIUM = 48;
constexpr ExtensionIcons EXTENSION_ICON_SMALL = 32;
constexpr ExtensionIcons EXTENSION_ICON_SMALLISH = 24;
constexpr ExtensionIcons EXTENSION_ICON_BITTY = 16;
constexpr ExtensionIcons EXTENSION_ICON_INVALID = 0;

// The extension id of the ChromeVox extension.
EXTENSIONS_EXPORT extern const char kChromeVoxExtensionId[];

// The extension id of the PDF extension.
EXTENSIONS_EXPORT extern const char kPdfExtensionId[];

// The extension id of the Office Viewer component extension.
EXTENSIONS_EXPORT extern const char kQuickOfficeComponentExtensionId[];

// The extension id of the Office Viewer extension on the internal webstore.
EXTENSIONS_EXPORT extern const char kQuickOfficeInternalExtensionId[];

// The extension id of the Office Viewer extension.
EXTENSIONS_EXPORT extern const char kQuickOfficeExtensionId[];

// The extension id used for testing mimeHandlerPrivate.
EXTENSIONS_EXPORT extern const char kMimeHandlerPrivateTestExtensionId[];

// The extension id of the Files Manager application.
EXTENSIONS_EXPORT extern const char kFilesManagerAppId[];

// The extension id of the Calculator application.
EXTENSIONS_EXPORT extern const char kCalculatorAppId[];

// The extension id of the demo Calendar application.
EXTENSIONS_EXPORT extern const char kCalendarDemoAppId[];

// The extension id of the GMail application.
EXTENSIONS_EXPORT extern const char kGmailAppId[];

// The extension id of the demo Google Docs application.
EXTENSIONS_EXPORT extern const char kGoogleDocsDemoAppId[];

// The extension id of the Google Docs PWA.
EXTENSIONS_EXPORT extern const char kGoogleDocsPwaAppId[];

// The extension id of the Google Drive application.
EXTENSIONS_EXPORT extern const char kGoogleDriveAppId[];

// The extension id of the Google Meet PWA.
EXTENSIONS_EXPORT extern const char kGoogleMeetPwaAppId[];

// The extension id of the demo Google Sheets application.
EXTENSIONS_EXPORT extern const char kGoogleSheetsDemoAppId[];

// The extension id of the Google Sheets PWA.
EXTENSIONS_EXPORT extern const char kGoogleSheetsPwaAppId[];

// The extension id of the demo Google Slides application.
EXTENSIONS_EXPORT extern const char kGoogleSlidesDemoAppId[];

// The extension id of the Google Keep application.
EXTENSIONS_EXPORT extern const char kGoogleKeepAppId[];

// The extension id of the Youtube application.
EXTENSIONS_EXPORT extern const char kYoutubeAppId[];

// The extension id of the Youtube PWA.
EXTENSIONS_EXPORT extern const char kYoutubePwaAppId[];

// The extension id of the Spotify PWA.
EXTENSIONS_EXPORT extern const char kSpotifyAppId[];

// The extension id of the BeFunky PWA.
EXTENSIONS_EXPORT extern const char kBeFunkyAppId[];

// The extension id of the Clipchamp PWA.
EXTENSIONS_EXPORT extern const char kClipchampAppId[];

// The extension id of the GeForce NOW PWA.
EXTENSIONS_EXPORT extern const char kGeForceNowAppId[];

// The extension id of the Zoom PWA.
EXTENSIONS_EXPORT extern const char kZoomAppId[];

// The extension id of the Sumo PWA.
EXTENSIONS_EXPORT extern const char kSumoAppId[];

// The extension id of the Sumo PWA.
EXTENSIONS_EXPORT extern const char kAdobeSparkAppId[];

// The extension id of the Google Docs application.
EXTENSIONS_EXPORT extern const char kGoogleDocsAppId[];

// The extension id of the Google Sheets application.
EXTENSIONS_EXPORT extern const char kGoogleSheetsAppId[];

// The extension id of the Google Slides application.
EXTENSIONS_EXPORT extern const char kGoogleSlidesAppId[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
// The id of the testing extension allowed in the signin profile.
EXTENSIONS_EXPORT extern const char kSigninProfileTestExtensionId[];

// The id of the testing extension allowed in guest mode.
EXTENSIONS_EXPORT extern const char kGuestModeTestExtensionId[];

// The id of the Chrome OS XKB extension.
EXTENSIONS_EXPORT extern const char kChromeOSXKB[];

// Returns true if this app is part of the "system UI". Generally this is UI
// that that on other operating systems would be considered part of the OS,
// for example the file manager.
EXTENSIONS_EXPORT bool IsSystemUIApp(base::StringPiece extension_id);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
// The extension id of the default Demo Mode Highlights app.
EXTENSIONS_EXPORT extern const char kHighlightsAppId[];

// The extension id of the default Demo Mode screensaver app.
EXTENSIONS_EXPORT extern const char kScreensaverAppId[];

// The extension id of 2022 Demo Mode Highlights app.
EXTENSIONS_EXPORT extern const char kNewAttractLoopAppId[];

// The extension id of 2022 Demo Mode screensaver app.
EXTENSIONS_EXPORT extern const char kNewHighlightsAppId[];

// Returns true if this app is one of Demo Mode Chrome Apps, including
// attract loop and highlights apps.
EXTENSIONS_EXPORT bool IsDemoModeChromeApp(base::StringPiece extension_id);
#endif  // BUILDFLAG(IS_CHROMEOS)

// True if the id matches any of the QuickOffice extension ids.
EXTENSIONS_EXPORT bool IsQuickOfficeExtension(const std::string& id);

// Returns if the app is managed by extension default apps. This is a hardcoded
// list of default apps for Windows/Linux/MacOS platforms that should be
// migrated from extension to web app.
// TODO(https://crbug.com/1257275): remove after deault app migration is done.
// This function is copied from
// chrome/browser/web_applications/extension_status_utils.h.
EXTENSIONS_EXPORT bool IsPreinstalledAppId(const std::string& app_id);

// The extension id for the production version of Hangouts.
EXTENSIONS_EXPORT extern const char kProdHangoutsExtensionId[];

// Extension ids used by Hangouts.
EXTENSIONS_EXPORT extern const char* const kHangoutsExtensionIds[6];

// Error message when enterprise policy blocks scripting of webpage.
EXTENSIONS_EXPORT extern const char kPolicyBlockedScripting[];

// Error message when access to incognito preferences is denied.
EXTENSIONS_EXPORT extern const char kIncognitoErrorMessage[];

// Error message when setting a pref with "incognito_session_only"
// scope is denied.
EXTENSIONS_EXPORT extern const char kIncognitoSessionOnlyErrorMessage[];

// Error message when an invalid color is provided to an API method.
EXTENSIONS_EXPORT extern const char kInvalidColorError[];

// The default block size for hashing used in content verification.
EXTENSIONS_EXPORT extern const int kContentVerificationDefaultBlockSize;

}  // namespace extension_misc

#endif  // EXTENSIONS_COMMON_CONSTANTS_H_
