// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_CONSTANTS_H_
#define EXTENSIONS_COMMON_CONSTANTS_H_

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "base/files/file_path.h"
#include "build/chromeos_buildflags.h"
#include "extensions/common/extensions_export.h"

namespace extensions {

// Scheme we serve extension content from.
inline constexpr char kExtensionScheme[] = "chrome-extension";

// URL used to indicate that an extension resource load request was invalid.
inline constexpr char kExtensionInvalidRequestURL[] =
    "chrome-extension://invalid/";

// The name of the manifest inside an extension.
inline constexpr base::FilePath::CharType kManifestFilename[] =
    FILE_PATH_LITERAL("manifest.json");

// The name of the differential fingerprint file inside an extension.
inline constexpr base::FilePath::CharType kDifferentialFingerprintFilename[] =
    FILE_PATH_LITERAL("manifest.fingerprint");

// The name of locale folder inside an extension.
inline constexpr base::FilePath::CharType kLocaleFolder[] =
    FILE_PATH_LITERAL("_locales");

// The name of the messages file inside an extension.
inline constexpr base::FilePath::CharType kMessagesFilename[] =
    FILE_PATH_LITERAL("messages.json");

// The name of the gzipped messages file inside an extension.
inline constexpr base::FilePath::CharType kGzippedMessagesFilename[] =
    FILE_PATH_LITERAL("messages.json.gz");

// The base directory for subdirectories with platform-specific code.
inline constexpr base::FilePath::CharType kPlatformSpecificFolder[] =
    FILE_PATH_LITERAL("_platform_specific");

// A directory reserved for metadata, generated either by the webstore
// or chrome.
inline constexpr base::FilePath::CharType kMetadataFolder[] =
    FILE_PATH_LITERAL("_metadata");

// Name of the verified contents file within the metadata folder.
inline constexpr base::FilePath::CharType kVerifiedContentsFilename[] =
    FILE_PATH_LITERAL("verified_contents.json");

// Name of the computed hashes file within the metadata folder.
inline constexpr base::FilePath::CharType kComputedHashesFilename[] =
    FILE_PATH_LITERAL("computed_hashes.json");

// Name of the indexed ruleset directory for the Declarative Net Request API.
inline constexpr base::FilePath::CharType kIndexedRulesetDirectory[] =
    FILE_PATH_LITERAL("generated_indexed_rulesets");

// The name of the directory inside the profile where extensions are
// installed to.
inline constexpr char kInstallDirectoryName[] = "Extensions";

// The name of the directory inside the profile where unpacked (e.g. from .zip
// file) extensions are installed to.
inline constexpr char kUnpackedInstallDirectoryName[] = "UnpackedExtensions";

// The name of a temporary directory to install an extension into for
// validation before finalizing install.
inline constexpr char kTempExtensionName[] = "CRX_INSTALL";

// The file to write our decoded message catalogs to, relative to the
// extension_path.
inline constexpr char kDecodedMessageCatalogsFilename[] =
    "DECODED_MESSAGE_CATALOGS";

// The filename to use for a background page generated from
// background.scripts.
inline constexpr char kGeneratedBackgroundPageFilename[] =
    "_generated_background_page.html";

// The URL piece between the extension ID and favicon URL.
inline constexpr char kFaviconSourcePath[] = "_favicon";

// Path to imported modules.
inline constexpr char kModulesDir[] = "_modules";

// The file extension (.crx) for extensions.
inline constexpr base::FilePath::CharType kExtensionFileExtension[] =
    FILE_PATH_LITERAL(".crx");

// The file extension (.pem) for private key files.
inline constexpr base::FilePath::CharType kExtensionKeyFileExtension[] =
    FILE_PATH_LITERAL(".pem");

// Default frequency for auto updates, if turned on (5 hours).
inline constexpr int kDefaultUpdateFrequencySeconds = 60 * 60 * 5;

// The name of the directory inside the profile where per-app local settings
// are stored.
inline constexpr base::FilePath::CharType kLocalAppSettingsDirectoryName[] =
    FILE_PATH_LITERAL("Local App Settings");

// The name of the directory inside the profile where per-extension local
// settings are stored.
inline constexpr base::FilePath::CharType
    kLocalExtensionSettingsDirectoryName[] =
        FILE_PATH_LITERAL("Local Extension Settings");

// The name of the directory inside the profile where per-app synced settings
// are stored.
inline constexpr base::FilePath::CharType kSyncAppSettingsDirectoryName[] =
    FILE_PATH_LITERAL("Sync App Settings");

// The name of the directory inside the profile where per-extension synced
// settings are stored.
inline constexpr base::FilePath::CharType
    kSyncExtensionSettingsDirectoryName[] =
        FILE_PATH_LITERAL("Sync Extension Settings");

// The name of the directory inside the profile where per-extension persistent
// managed settings are stored.
inline constexpr base::FilePath::CharType kManagedSettingsDirectoryName[] =
    FILE_PATH_LITERAL("Managed Extension Settings");

// The name of the database inside the profile where chrome-internal
// extension state resides.
inline constexpr base::FilePath::CharType kStateStoreName[] =
    FILE_PATH_LITERAL("Extension State");

// The name of the database inside the profile where declarative extension
// rules are stored.
inline constexpr base::FilePath::CharType kRulesStoreName[] =
    FILE_PATH_LITERAL("Extension Rules");

// The name of the database inside the profile where persistent dynamic user
// script metadata is stored.
inline constexpr base::FilePath::CharType kScriptsStoreName[] =
    FILE_PATH_LITERAL("Extension Scripts");

// Statistics are logged to UMA with these strings as part of histogram name.
// They can all be found under Extensions.Database.Open.<client>. Changing this
// needs to synchronize with histograms.xml, AND will also become incompatible
// with older browsers still reporting the previous values.
inline constexpr char kSettingsDatabaseUMAClientName[] = "Settings";
inline constexpr char kRulesDatabaseUMAClientName[] = "Rules";
inline constexpr char kStateDatabaseUMAClientName[] = "State";
inline constexpr char kScriptsDatabaseUMAClientName[] = "Scripts";

// Mime type strings
inline constexpr char kMimeTypeJpeg[] = "image/jpeg";
inline constexpr char kMimeTypePng[] = "image/png";

// The extension id of the Web Store component application.
inline constexpr char kWebStoreAppId[] = "ahfgeienlihckogmohjhadlkjgocpleb";

// The key used for signing some pieces of data from the webstore.
EXTENSIONS_EXPORT extern const uint8_t kWebstoreSignaturesPublicKey[];
EXTENSIONS_EXPORT extern const size_t kWebstoreSignaturesPublicKeySize;

// A preference for storing the extension's update URL data.
inline constexpr char kUpdateURLData[] = "update_url_data";

// Thread identifier for the main renderer thread (as opposed to a service
// worker thread).
// This is the default thread id used for extension event listeners registered
// from a non-service worker context
inline constexpr int kMainThreadId = 0;

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
  kSourceFocusMode = 28,        // App launch from Focus Mode panel.
  kSourceSparky = 29,           // App launch from Sparky.

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kSourceSparky,
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
inline constexpr int kUnknownTabId = -1;

// Matches chrome.windows.WINDOW_ID_NONE.
inline constexpr int kUnknownWindowId = -1;

// Matches chrome.windows.WINDOW_ID_CURRENT.
inline constexpr int kCurrentWindowId = -2;

using ExtensionIcons = int;
inline constexpr ExtensionIcons EXTENSION_ICON_GIGANTOR = 512;
inline constexpr ExtensionIcons EXTENSION_ICON_EXTRA_LARGE = 256;
inline constexpr ExtensionIcons EXTENSION_ICON_LARGE = 128;
inline constexpr ExtensionIcons EXTENSION_ICON_MEDIUM = 48;
inline constexpr ExtensionIcons EXTENSION_ICON_SMALL = 32;
inline constexpr ExtensionIcons EXTENSION_ICON_SMALLISH = 24;
inline constexpr ExtensionIcons EXTENSION_ICON_BITTY = 16;
inline constexpr ExtensionIcons EXTENSION_ICON_INVALID = 0;

// The extension id of the ChromeVox extension.
inline constexpr char kChromeVoxExtensionId[] =
#if BUILDFLAG(IS_CHROMEOS)
    // The extension id for the built-in component extension.
    "mndnfokpggljbaajbnioimlmbfngpief";
#else
    // The extension id for the web store extension.
    "kgejglhpjiefppelpmljglcjbhoiplfn";
#endif

// The extension id of the PDF extension.
inline constexpr char kPdfExtensionId[] = "mhjfbmdgcfjbbpaeojofohoefgiehjai";

// The extension id of the Office Viewer component extension.
inline constexpr char kQuickOfficeComponentExtensionId[] =
    "bpmcpldpdmajfigpchkicefoigmkfalc";

// The extension id of the Office Viewer extension on the internal webstore.
inline constexpr char kQuickOfficeInternalExtensionId[] =
    "ehibbfinohgbchlgdbfpikodjaojhccn";

// The extension id of the Office Viewer extension.
inline constexpr char kQuickOfficeExtensionId[] =
    "gbkeegbaiigmenfmjfclcdgdpimamgkj";

// The extension id used for testing mimeHandlerPrivate.
inline constexpr char kMimeHandlerPrivateTestExtensionId[] =
    "oickdpebdnfbgkcaoklfcdhjniefkcji";

// The extension id of the Files Manager application.
inline constexpr char kFilesManagerAppId[] = "hhaomjibdihmijegdhdafkllkbggdgoj";

// The extension id of the Calculator application.
inline constexpr char kCalculatorAppId[] = "joodangkbfjnajiiifokapkpmhfnpleo";

// The extension id of the demo Calendar application.
inline constexpr char kCalendarDemoAppId[] = "fpgfohogebplgnamlafljlcidjedbdeb";

// The extension id of the GMail application.
inline constexpr char kGmailAppId[] = "pjkljhegncpnkpknbcohdijeoejaedia";

// The extension id of the demo Google Docs application.
inline constexpr char kGoogleDocsDemoAppId[] =
    "chdaoodbokekbiiphekbfjdmiodccljl";

// The extension id of the Google Docs PWA.
inline constexpr char kGoogleDocsPwaAppId[] =
    "cepkndkdlbllfhpfhledabdcdbidehkd";

// The extension id of the Google Drive application.
inline constexpr char kGoogleDriveAppId[] = "apdfllckaahabafndbhieahigkjlhalf";

// The extension id of the Google Meet PWA.
inline constexpr char kGoogleMeetPwaAppId[] =
    "dkainijpcknoofiakgccliajhbmlbhji";

// The extension id of the demo Google Sheets application.
inline constexpr char kGoogleSheetsDemoAppId[] =
    "nifkmgcdokhkjghdlgflonppnefddien";

// The extension id of the Google Sheets PWA.
inline constexpr char kGoogleSheetsPwaAppId[] =
    "hcgjdbbnhkmopplfiibmdgghhdhbiidh";

// The extension id of the demo Google Slides application.
inline constexpr char kGoogleSlidesDemoAppId[] =
    "hdmobeajeoanbanmdlabnbnlopepchip";

// The extension id of the Google Keep application.
inline constexpr char kGoogleKeepAppId[] = "hmjkmjkepdijhoojdojkdfohbdgmmhki";

// The extension id of the Youtube application.
inline constexpr char kYoutubeAppId[] = "blpcfgokakmgnkcojhhkbfbldkacnbeo";

// The extension id of the Youtube PWA.
inline constexpr char kYoutubePwaAppId[] = "agimnkijcaahngcdmfeangaknmldooml";

// The extension id of the Spotify PWA.
inline constexpr char kSpotifyAppId[] = "pjibgclleladliembfgfagdaldikeohf";

// The extension id of the BeFunky PWA.
inline constexpr char kBeFunkyAppId[] = "fjoomcalbeohjbnlcneddljemclcekeg";

// The extension id of the Clipchamp PWA.
inline constexpr char kClipchampAppId[] = "pfepfhbcedkbjdkanpimmmdjfgoddhkg";

// The extension id of the GeForce NOW PWA.
inline constexpr char kGeForceNowAppId[] = "egmafekfmcnknbdlbfbhafbllplmjlhn";

// The extension id of the Zoom PWA.
inline constexpr char kZoomAppId[] = "jldpdkiafafcejhceeincjmlkmibemgj";

// The extension id of the Sumo PWA.
inline constexpr char kSumoAppId[] = "mfknjekfflbfdchhohffdpkokgfbfmdc";

// The extension id of the Sumo PWA.
inline constexpr char kAdobeSparkAppId[] = "magefboookdoiehjohjmbjmkepngibhm";

// The extension id of the Google Docs application.
inline constexpr char kGoogleDocsAppId[] = "aohghmighlieiainnegkcijnfilokake";

// The extension id of the Google Sheets application.
inline constexpr char kGoogleSheetsAppId[] = "felcaaldnbdncclmgdcncolpebgiejap";

// The extension id of the Google Slides application.
inline constexpr char kGoogleSlidesAppId[] = "aapocclcgogkmnckokdopfmhonfmgoek";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// The id of the testing extension allowed in the signin profile.
inline constexpr char kSigninProfileTestExtensionId[] =
    "mecfefiddjlmabpeilblgegnbioikfmp";

// The id of the testing extension allowed in guest mode.
inline constexpr char kGuestModeTestExtensionId[] =
    "behllobkkfkfnphdnhnkndlbkcpglgmj";

// The extension id of the Amazon Luna .ca Canada PWA.
inline constexpr char kAmazonLunaAppIdCA[] = "agmpcdnpkedhhjldepagpgebdindblfd";

// The extension id of the Amazon Luna .de Germany PWA.
inline constexpr char kAmazonLunaAppIdDE[] = "lhedecbjcehgjijkmihhhfmdicbkkgkm";

// The extension id of the Amazon Luna .es Spain PWA.
inline constexpr char kAmazonLunaAppIdES[] = "befdkfemegjbohkncpbchjcgndhgajfg";

// The extension id of the Amazon Luna .fr France PWA.
inline constexpr char kAmazonLunaAppIdFR[] = "khklcoifabacgdieoekhmfcilgfmdmbh";

// The extension id of the Amazon Luna .it Italy PWA.
inline constexpr char kAmazonLunaAppIdIT[] = "agcdabkknemgfgbjdpckaehhncgkfcdi";

// The extension id of the Amazon Luna .nl Netherlands PWA.
inline constexpr char kAmazonLunaAppIdNL[] = "opkohmiamoeiojmgmhgelaaieecjifod";

// The extension id of the Amazon Luna .pl Poland PWA.
inline constexpr char kAmazonLunaAppIdPL[] = "alddamigfjonblpigkpieckmhbjdgadd";

// The extension id of the Amazon Luna .co.uk UK PWA.
inline constexpr char kAmazonLunaAppIdUK[] = "aolalpmkbpdlpjhmhhmcobipjkhlimkj";

// The extension id of the Amazon Luna .com US PWA.
inline constexpr char kAmazonLunaAppIdUS[] = "mdjpfbokiopdhidmalnpnmekjbajopld";

// The extension id of the Boosteroid PWA.
inline constexpr char kBoosteroidAppId[] = "ncjnbebeamfkkddkofiijnlpkcnobgin";

// The extension id of the Cool Math Games PWA.
inline constexpr char kCoolMathGamesAppId[] =
    "moflhbhdponafajiefoaamnkbhpigdoc";

// The extension id of the Now.gg UK PWA.
inline constexpr char kNowGGAppIdUK[] = "nphngfagcmpkdicidafibmfcijfighif";

// The extension id of the Now.gg US PWA.
inline constexpr char kNowGGAppIdUS[] = "dgfmnbibgdaghllenpkjalbnljbffabj";

// The extension id of the Poki PWA.
inline constexpr char kPokiAppId[] = "nccldcgjjeeglpgcgebibmhmkakanigi";

// The extension id of the Xbox Cloud Gaming PWA.
inline constexpr char kXboxCloudGamingAppId[] =
    "chcecgcbjkilfgeccdhoeaillkophnhg";

// Returns true if this app is part of the "system UI". Generally this is UI
// that that on other operating systems would be considered part of the OS,
// for example the file manager.
EXTENSIONS_EXPORT bool IsSystemUIApp(std::string_view extension_id);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
// The extension id of the default Demo Mode Highlights app.
inline constexpr char kHighlightsAppId[] = "lpmakjfjcconjeehbidjclhdlpjmfjjj";

// The extension id of the default Demo Mode screensaver app.
inline constexpr char kScreensaverAppId[] = "mnoijifedipmbjaoekhadjcijipaijjc";

// The extension id of 2022 Demo Mode Highlights app.
inline constexpr char kNewAttractLoopAppId[] =
    "igilkdghcdehjdcpndaodgnjgdggiemm";

// The extension id of 2022 Demo Mode screensaver app.
inline constexpr char kNewHighlightsAppId[] =
    "enchmnkoajljphdmahljlebfmpkkbnkj";

// Returns true if this app is one of Demo Mode Chrome Apps, including
// attract loop and highlights apps.
EXTENSIONS_EXPORT bool IsDemoModeChromeApp(std::string_view extension_id);
#endif  // BUILDFLAG(IS_CHROMEOS)

// True if the id matches any of the QuickOffice extension ids.
EXTENSIONS_EXPORT bool IsQuickOfficeExtension(std::string_view extension_id);

// Returns if the app is managed by extension default apps. This is a hardcoded
// list of default apps for Windows/Linux/MacOS platforms that should be
// migrated from extension to web app.
// TODO(crbug.com/40796281): remove after deault app migration is done.
// This function is copied from
// chrome/browser/web_applications/extension_status_utils.h.
EXTENSIONS_EXPORT bool IsPreinstalledAppId(std::string_view app_id);

// Error message when enterprise policy blocks scripting of webpage.
inline constexpr char kPolicyBlockedScripting[] =
    "This page cannot be scripted due to an ExtensionsSettings policy.";

// Error message when access to incognito preferences is denied.
inline constexpr char kIncognitoErrorMessage[] =
    "You do not have permission to access incognito preferences.";

// Error message when setting a pref with "incognito_session_only"
// scope is denied.
inline constexpr char kIncognitoSessionOnlyErrorMessage[] =
    "You cannot set a preference with scope 'incognito_session_only' when no "
    "incognito window is open.";

// Error message when an invalid color is provided to an API method.
inline constexpr char kInvalidColorError[] =
    "The color specification could not be parsed.";

// The default block size for hashing used in content verification.
inline constexpr int kContentVerificationDefaultBlockSize = 4096;

// The extension id of the Google Docs Offline extension.
// TODO(crbug.com/325613709): This is only used to log targeted histograms to
// diagnose corruption rates for this extension. Move this back to
// chrome/common/extensions/extension_constants.h once the issue has been
// resolved.
inline constexpr char kDocsOfflineExtensionId[] =
    "ghbmnnjooekpmoecnnnilnnbdlolhkhi";

// This is used extensively, generally as a key in a dictionary.
inline constexpr char kId[] = "id";

}  // namespace extension_misc

#endif  // EXTENSIONS_COMMON_CONSTANTS_H_
