// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_API_PERMISSION_H_
#define EXTENSIONS_COMMON_PERMISSIONS_API_PERMISSION_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/pickle.h"
#include "base/values.h"

namespace extensions {

class PermissionIDSet;
class APIPermissionInfo;
class PermissionsInfo;

// APIPermission is for handling some complex permissions. Please refer to
// extensions::SocketPermission as an example.
// There is one instance per permission per loaded extension.
class APIPermission {
 public:
  // The IDs of all permissions available to apps. Add as many permissions here
  // as needed to generate meaningful permission messages. Add the rules for the
  // messages to ChromePermissionMessageProvider.
  // Do not reorder this enumeration or remove any entries. To deprecate an
  // entry, prefix it with the "kDeleted_" specifier and to add a new entry, add
  // it just prior to kEnumBoundary, and ensure to update the
  // "ExtensionPermission3" enum in tools/metrics/histograms/histograms.xml (by
  // running update_extension_permission.py).
  // TODO(sashab): Move this to a more central location, and rename it to
  // PermissionID.
  enum ID {
    // Error codes.
    kInvalid = 0,
    kUnknown = 1,

    // Actual permission IDs. Not all of these are valid permissions on their
    // own; some are just needed by various manifest permissions to represent
    // their permission message rule combinations.
    kAccessibilityFeaturesModify = 2,
    kAccessibilityFeaturesRead = 3,
    kAccessibilityPrivate = 4,
    kActiveTab = 5,
    kActivityLogPrivate = 6,
    kAlarms = 7,
    kAlphaEnabled = 8,
    kAlwaysOnTopWindows = 9,
    kAppView = 10,
    kAudio = 11,
    kAudioCapture = 12,
    kDeleted_AudioModem = 13,
    kAutofillPrivate = 14,
    kAutomation = 15,
    kAutoTestPrivate = 16,
    kBackground = 17,
    kBluetoothPrivate = 18,
    kBookmark = 19,
    kBookmarkManagerPrivate = 20,
    kBrailleDisplayPrivate = 21,
    kBrowser = 22,
    kBrowsingData = 23,
    kCast = 24,
    kDeleted_kCastStreaming = 25,
    kChromeosInfoPrivate = 26,
    kClipboardRead = 27,
    kClipboardWrite = 28,
    kCloudPrintPrivate = 29,
    kCommandLinePrivate = 30,
    kCommandsAccessibility = 31,
    kContentSettings = 32,
    kContextMenus = 33,
    kCookie = 34,
    kDeleted_Copresence = 35,
    kDeleted_CopresencePrivate = 36,
    kCryptotokenPrivate = 37,
    kDataReductionProxy = 38,
    kDiagnostics = 39,
    kDeleted_Dial = 40,  // API removed.
    kDebugger = 41,
    kDeclarative = 42,
    kDeclarativeContent = 43,
    kDeclarativeWebRequest = 44,
    kDesktopCapture = 45,
    kDesktopCapturePrivate = 46,
    kDeveloperPrivate = 47,
    kDevtools = 48,
    kDns = 49,
    kDocumentScan = 50,
    kDownloads = 51,
    kDownloadsInternal = 52,
    kDownloadsOpen = 53,
    kDownloadsShelf = 54,
    kDeleted_EasyUnlockPrivate = 55,
    kEchoPrivate = 56,
    kDeleted_EmbeddedExtensionOptions = 57,
    kEnterprisePlatformKeys = 58,
    kEnterprisePlatformKeysPrivate = 59,
    kDeleted_ExperienceSamplingPrivate = 60,
    kExperimental = 61,
    kDeleted_ExtensionView = 62,  // crbug.com/982858
    kExternallyConnectableAllUrls = 63,
    kFeedbackPrivate = 64,
    kFileBrowserHandler = 65,
    kFileBrowserHandlerInternal = 66,
    kFileManagerPrivate = 67,
    kFileSystem = 68,
    kFileSystemDirectory = 69,
    kFileSystemProvider = 70,
    kFileSystemRequestFileSystem = 71,
    kFileSystemRetainEntries = 72,
    kFileSystemWrite = 73,
    kDeleted_FileSystemWriteDirectory = 74,
    kFirstRunPrivate = 75,
    kFontSettings = 76,
    kFullscreen = 77,
    kDeleted_GcdPrivate = 78,
    kGcm = 79,
    kGeolocation = 80,
    kHid = 81,
    kHistory = 82,
    kHomepage = 83,
    kHotwordPrivate = 84,
    kIdentity = 85,
    kIdentityEmail = 86,
    kIdentityPrivate = 87,
    kIdltest = 88,
    kIdle = 89,
    kImeWindowEnabled = 90,
    kDeleted_InlineInstallPrivate = 91,
    kInput = 92,
    kInputMethodPrivate = 93,
    kDeleted_InterceptAllKeys = 94,
    kLauncherSearchProvider = 95,
    kLocation = 96,
    kDeleted_LogPrivate = 97,
    kManagement = 98,
    kMediaGalleries = 99,
    kMediaPlayerPrivate = 100,
    kMediaRouterPrivate = 101,
    kMetricsPrivate = 102,
    kMDns = 103,
    kMusicManagerPrivate = 104,
    kNativeMessaging = 105,
    kNetworkingConfig = 106,
    kNetworkingPrivate = 107,
    kDeleted_NotificationProvider = 108,
    kNotifications = 109,
    kOverrideEscFullscreen = 110,
    kPageCapture = 111,
    kPointerLock = 112,
    kPlatformKeys = 113,
    kDeleted_Plugin = 114,
    kPower = 115,
    kDeleted_PreferencesPrivate = 116,
    kDeleted_PrincipalsPrivate = 117,
    kPrinterProvider = 118,
    kPrivacy = 119,
    kProcesses = 120,
    kProxy = 121,
    kImageWriterPrivate = 122,
    kDeleted_ReadingListPrivate = 123,
    kRtcPrivate = 124,
    kSearchProvider = 125,
    kSearchEnginesPrivate = 126,
    kSerial = 127,
    kSessions = 128,
    kSettingsPrivate = 129,
    kSignedInDevices = 130,
    kSocket = 131,
    kStartupPages = 132,
    kStorage = 133,
    kDeleted_StreamsPrivate = 134,
    kSyncFileSystem = 135,
    kSystemPrivate = 136,
    kSystemDisplay = 137,
    kSystemStorage = 138,
    kTab = 139,
    kTabCapture = 140,
    kTabCaptureForTab = 141,
    kTerminalPrivate = 142,
    kTopSites = 143,
    kTts = 144,
    kTtsEngine = 145,
    kUnlimitedStorage = 146,
    kU2fDevices = 147,
    kUsb = 148,
    kUsbDevice = 149,
    kVideoCapture = 150,
    kVirtualKeyboardPrivate = 151,
    kVpnProvider = 152,
    kWallpaper = 153,
    kWallpaperPrivate = 154,
    kWebcamPrivate = 155,
    kDeleted_kWebConnectable = 156,  // for externally_connectable manifest key
    kWebNavigation = 157,
    kWebRequest = 158,
    kWebRequestBlocking = 159,
    kWebrtcAudioPrivate = 160,
    kWebrtcDesktopCapturePrivate = 161,
    kWebrtcLoggingPrivate = 162,
    kWebstorePrivate = 163,
    kWebstoreWidgetPrivate = 164,
    kWebView = 165,
    kWindowShape = 166,
    kDeleted_ScreenlockPrivate = 167,
    kSystemCpu = 168,
    kSystemMemory = 169,
    kSystemNetwork = 170,
    kSystemInfoCpu = 171,
    kSystemInfoMemory = 172,
    kBluetooth = 173,
    kBluetoothDevices = 174,
    kFavicon = 175,
    kFullAccess = 176,
    kHostReadOnly = 177,
    kHostReadWrite = 178,
    kHostsAll = 179,
    kHostsAllReadOnly = 180,
    kMediaGalleriesAllGalleriesCopyTo = 181,
    kMediaGalleriesAllGalleriesDelete = 182,
    kMediaGalleriesAllGalleriesRead = 183,
    kNetworkState = 184,
    kDeleted_OverrideBookmarksUI = 185,
    kShouldWarnAllHosts = 186,
    kSocketAnyHost = 187,
    kSocketDomainHosts = 188,
    kSocketSpecificHosts = 189,
    kDeleted_UsbDeviceList = 190,
    kUsbDeviceUnknownProduct = 191,
    kUsbDeviceUnknownVendor = 192,
    kUsersPrivate = 193,
    kPasswordsPrivate = 194,
    kLanguageSettingsPrivate = 195,
    kEnterpriseDeviceAttributes = 196,
    kCertificateProvider = 197,
    kResourcesPrivate = 198,
    kDisplaySource = 199,
    kClipboard = 200,
    kNetworkingOnc = 201,
    kVirtualKeyboard = 202,
    kNetworkingCastPrivate = 203,
    kMediaPerceptionPrivate = 204,
    kLockScreen = 205,
    kNewTabPageOverride = 206,
    kDeclarativeNetRequest = 207,
    kLockWindowFullscreenPrivate = 208,
    kWebrtcLoggingPrivateAudioDebug = 209,
    kEnterpriseReportingPrivate = 210,
    kCecPrivate = 211,
    kSafeBrowsingPrivate = 212,
    kFileSystemRequestDownloads = 213,
    kSystemPowerSource = 214,
    kArcAppsPrivate = 215,
    kEnterpriseHardwarePlatform = 216,
    kLoginScreenUi = 217,
    kDeclarativeNetRequestFeedback = 218,
    kTransientBackground = 219,
    kLogin = 220,
    kLoginScreenStorage = 221,
    kLoginState = 222,
    kPrintingMetrics = 223,
    kPrinting = 224,
    kCrashReportPrivate = 225,
    kAutofillAssistantPrivate = 226,
    kEnterpriseNetworkingAttributes = 227,
    kSearch = 228,
    // Last entry: Add new entries above and ensure to update the
    // "ExtensionPermission3" enum in tools/metrics/histograms/enums.xml
    // (by running update_extension_permission.py).
    kEnumBoundary,
  };

  struct CheckParam {
  };

  explicit APIPermission(const APIPermissionInfo* info);

  virtual ~APIPermission();

  // Returns the id of this permission.
  ID id() const;

  // Returns the name of this permission.
  const char* name() const;

  // Returns the APIPermission of this permission.
  const APIPermissionInfo* info() const {
    return info_;
  }

  // The set of permissions an app/extension with this API permission has. These
  // permissions are used by PermissionMessageProvider to generate meaningful
  // permission messages for the app/extension.
  //
  // For simple API permissions, this will return a set containing only the ID
  // of the permission. More complex permissions might have multiple IDs, one
  // for each of the capabilities the API permission has (e.g. read, write and
  // copy, in the case of the media gallery permission). Permissions that
  // require parameters may also contain a parameter string (along with the
  // permission's ID) which can be substituted into the permission message if a
  // rule is defined to do so.
  //
  // Permissions with multiple values, such as host permissions, are represented
  // by multiple entries in this set. Each permission in the subset has the same
  // ID (e.g. kHostReadOnly) but a different parameter (e.g. google.com). These
  // are grouped to form different kinds of permission messages (e.g. 'Access to
  // 2 hosts') depending on the number that are in the set. The rules that
  // define the grouping of related permissions with the same ID is defined in
  // ChromePermissionMessageProvider.
  virtual PermissionIDSet GetPermissions() const = 0;

  // Returns true if the given permission is allowed.
  virtual bool Check(const CheckParam* param) const = 0;

  // Returns true if |rhs| is a subset of this.
  virtual bool Contains(const APIPermission* rhs) const = 0;

  // Returns true if |rhs| is equal to this.
  virtual bool Equal(const APIPermission* rhs) const = 0;

  // Parses the APIPermission from |value|. Returns false if an error happens
  // and optionally set |error| if |error| is not NULL. If |value| represents
  // multiple permissions, some are invalid, and |unhandled_permissions| is
  // not NULL, the invalid ones are put into |unhandled_permissions| and the
  // function returns true.
  virtual bool FromValue(const base::Value* value,
                         std::string* error,
                         std::vector<std::string>* unhandled_permissions) = 0;

  // Stores this into a new created |value|.
  virtual std::unique_ptr<base::Value> ToValue() const = 0;

  // Clones this.
  virtual std::unique_ptr<APIPermission> Clone() const = 0;

  // Returns a new API permission which equals this - |rhs|.
  virtual std::unique_ptr<APIPermission> Diff(
      const APIPermission* rhs) const = 0;

  // Returns a new API permission which equals the union of this and |rhs|.
  virtual std::unique_ptr<APIPermission> Union(
      const APIPermission* rhs) const = 0;

  // Returns a new API permission which equals the intersect of this and |rhs|.
  virtual std::unique_ptr<APIPermission> Intersect(
      const APIPermission* rhs) const = 0;

  // IPC functions
  // Writes this into the given IPC message |m|.
  virtual void Write(base::Pickle* m) const = 0;

  // Reads from the given IPC message |m|.
  virtual bool Read(const base::Pickle* m, base::PickleIterator* iter) = 0;

  // Logs this permission.
  virtual void Log(std::string* log) const = 0;

 private:
  const APIPermissionInfo* const info_;
};


// The APIPermissionInfo is an immutable class that describes a single
// named permission (API permission).
// There is one instance per permission.
class APIPermissionInfo {
 public:
  enum Flag {
    kFlagNone = 0,

    // Plugins (NPAPI) are deprecated.
    // kFlagImpliesFullAccess = 1 << 0,

    // Indicates if the permission implies full URL access.
    kFlagImpliesFullURLAccess = 1 << 1,

    // Indicates that extensions cannot specify the permission as optional.
    kFlagCannotBeOptional = 1 << 3,

    // Indicates that the permission is internal to the extensions
    // system and cannot be specified in the "permissions" list.
    kFlagInternal = 1 << 4,

    // Indicates that the permission may be granted to web contents by
    // extensions using the content_capabilities manifest feature.
    kFlagSupportsContentCapabilities = 1 << 5,

    // Indicates whether the permission should trigger one of the powerful
    // permissions messages in chrome://management. Reach out to the privacy
    // team when you add a new permission to check whether you should set this
    // flag or not.
    kFlagRequiresManagementUIWarning = 1 << 6,

    // Indicates that the permission shouldn't trigger the full warning on
    // the login screen of the managed-guest session. See
    // prefs::kManagedSessionUseFullLoginWarning. Most permissions are
    // considered powerful enough to warrant the full warning,
    // so the default for permissions (by not including this flag) is to trigger
    // it. Reach out to the privacy team when you add a new permission to check
    // whether you should set this flag or not.
    kFlagDoesNotRequireManagedSessionFullLoginWarning = 1 << 7
  };

  using APIPermissionConstructor =
      std::unique_ptr<APIPermission> (*)(const APIPermissionInfo*);

  typedef std::set<APIPermission::ID> IDSet;

  // This exists to allow aggregate initialization, so that default values
  // for flags, etc. can be omitted.
  // TODO(yoz): Simplify the way initialization is done. APIPermissionInfo
  // should be the simple data struct.
  struct InitInfo {
    APIPermission::ID id;
    const char* name;
    int flags;
    APIPermissionInfo::APIPermissionConstructor constructor;
  };

  ~APIPermissionInfo();

  // Creates a APIPermission instance.
  std::unique_ptr<APIPermission> CreateAPIPermission() const;

  int flags() const { return flags_; }

  APIPermission::ID id() const { return id_; }

  // Returns the name of this permission.
  const char* name() const { return name_; }

  // Returns true if this permission implies full URL access.
  bool implies_full_url_access() const {
    return (flags_ & kFlagImpliesFullURLAccess) != 0;
  }

  // Returns true if this permission can be added and removed via the
  // optional permissions extension API.
  bool supports_optional() const {
    return (flags_ & kFlagCannotBeOptional) == 0;
  }

  // Returns true if this permission is internal rather than a
  // "permissions" list entry.
  bool is_internal() const {
    return (flags_ & kFlagInternal) != 0;
  }

  // Returns true if this permission can be granted to web contents by an
  // extension through the content_capabilities manifest feature.
  bool supports_content_capabilities() const {
    return (flags_ & kFlagSupportsContentCapabilities) != 0;
  }

  // Returns true if this permission should trigger a warning on the management
  // page.
  bool requires_management_ui_warning() const {
    return (flags_ & kFlagRequiresManagementUIWarning) != 0;
  }

  // Returns true if this permission should trigger the full warning on the
  // login screen of the managed guest session.
  bool requires_managed_session_full_login_warning() const {
    return (flags_ & kFlagDoesNotRequireManagedSessionFullLoginWarning) == 0;
  }

 private:
  // Instances should only be constructed from within a PermissionsInfo.
  friend class PermissionsInfo;
  // Implementations of APIPermission will want to get the permission message,
  // but this class's implementation should be hidden from everyone else.
  friend class APIPermission;

  explicit APIPermissionInfo(const InitInfo& info);

  const char* const name_;
  const APIPermission::ID id_;
  const int flags_;
  const APIPermissionConstructor api_permission_constructor_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_API_PERMISSION_H_
