// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/api_permission.h"

#include "extensions/common/permissions/api_permission_set.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using extensions::APIPermission;
using extensions::APIPermissionInfo;
using extensions::mojom::APIPermissionID;

class SimpleAPIPermission : public APIPermission {
 public:
  explicit SimpleAPIPermission(const APIPermissionInfo* permission)
    : APIPermission(permission) { }

  ~SimpleAPIPermission() override {}

  extensions::PermissionIDSet GetPermissions() const override {
    extensions::PermissionIDSet permissions;
    permissions.insert(static_cast<extensions::mojom::APIPermissionID>(id()));
    return permissions;
  }

  bool Check(const APIPermission::CheckParam* param) const override {
    return !param;
  }

  bool Contains(const APIPermission* rhs) const override {
    CHECK_EQ(info(), rhs->info());
    return true;
  }

  bool Equal(const APIPermission* rhs) const override {
    if (this != rhs)
      CHECK_EQ(info(), rhs->info());
    return true;
  }

  bool FromValue(const base::Value* value,
                 std::string* /*error*/,
                 std::vector<std::string>* /*unhandled_permissions*/) override {
    return (value == NULL);
  }

  std::unique_ptr<base::Value> ToValue() const override {
    return std::unique_ptr<base::Value>();
  }

  std::unique_ptr<APIPermission> Clone() const override {
    return std::make_unique<SimpleAPIPermission>(info());
  }

  std::unique_ptr<APIPermission> Diff(const APIPermission* rhs) const override {
    CHECK_EQ(info(), rhs->info());
    return nullptr;
  }

  std::unique_ptr<APIPermission> Union(
      const APIPermission* rhs) const override {
    CHECK_EQ(info(), rhs->info());
    return std::make_unique<SimpleAPIPermission>(info());
  }

  std::unique_ptr<APIPermission> Intersect(
      const APIPermission* rhs) const override {
    CHECK_EQ(info(), rhs->info());
    return std::make_unique<SimpleAPIPermission>(info());
  }

  void Write(base::Pickle* m) const override {}

  bool Read(const base::Pickle* m, base::PickleIterator* iter) override {
    return true;
  }

  void Log(std::string* log) const override {}
};

// Verifies that extensions::APIPermission::ID and
// extensions::mojom::APIPermissionID are kept in sync.
// This static asserts will be removed after replacing all APIPermission::ID
// enum with APIPermissionID Mojo enum.
#define STATIC_ASSERT_ENUM(value)                             \
  static_assert(static_cast<int>(APIPermission::ID::value) == \
                    static_cast<int>(APIPermissionID::value), \
                "mismatching enums: APIPermission::ID::" #value)

STATIC_ASSERT_ENUM(kInvalid);
STATIC_ASSERT_ENUM(kUnknown);
STATIC_ASSERT_ENUM(kAccessibilityFeaturesModify);
STATIC_ASSERT_ENUM(kAccessibilityFeaturesRead);
STATIC_ASSERT_ENUM(kAccessibilityPrivate);
STATIC_ASSERT_ENUM(kActiveTab);
STATIC_ASSERT_ENUM(kActivityLogPrivate);
STATIC_ASSERT_ENUM(kAlarms);
STATIC_ASSERT_ENUM(kAlphaEnabled);
STATIC_ASSERT_ENUM(kAlwaysOnTopWindows);
STATIC_ASSERT_ENUM(kAppView);
STATIC_ASSERT_ENUM(kAudio);
STATIC_ASSERT_ENUM(kAudioCapture);
STATIC_ASSERT_ENUM(kDeleted_AudioModem);
STATIC_ASSERT_ENUM(kAutofillPrivate);
STATIC_ASSERT_ENUM(kAutomation);
STATIC_ASSERT_ENUM(kAutoTestPrivate);
STATIC_ASSERT_ENUM(kBackground);
STATIC_ASSERT_ENUM(kBluetoothPrivate);
STATIC_ASSERT_ENUM(kBookmark);
STATIC_ASSERT_ENUM(kBookmarkManagerPrivate);
STATIC_ASSERT_ENUM(kBrailleDisplayPrivate);
STATIC_ASSERT_ENUM(kBrowser);
STATIC_ASSERT_ENUM(kBrowsingData);
STATIC_ASSERT_ENUM(kCast);
STATIC_ASSERT_ENUM(kDeleted_kCastStreaming);
STATIC_ASSERT_ENUM(kChromeosInfoPrivate);
STATIC_ASSERT_ENUM(kClipboardRead);
STATIC_ASSERT_ENUM(kClipboardWrite);
STATIC_ASSERT_ENUM(kDeleted_CloudPrintPrivate);
STATIC_ASSERT_ENUM(kCommandLinePrivate);
STATIC_ASSERT_ENUM(kCommandsAccessibility);
STATIC_ASSERT_ENUM(kContentSettings);
STATIC_ASSERT_ENUM(kContextMenus);
STATIC_ASSERT_ENUM(kCookie);
STATIC_ASSERT_ENUM(kDeleted_Copresence);
STATIC_ASSERT_ENUM(kDeleted_CopresencePrivate);
STATIC_ASSERT_ENUM(kCryptotokenPrivate);
STATIC_ASSERT_ENUM(kDeleted_DataReductionProxy);
STATIC_ASSERT_ENUM(kDiagnostics);
STATIC_ASSERT_ENUM(kDeleted_Dial);
STATIC_ASSERT_ENUM(kDebugger);
STATIC_ASSERT_ENUM(kDeclarative);
STATIC_ASSERT_ENUM(kDeclarativeContent);
STATIC_ASSERT_ENUM(kDeclarativeWebRequest);
STATIC_ASSERT_ENUM(kDesktopCapture);
STATIC_ASSERT_ENUM(kDesktopCapturePrivate);
STATIC_ASSERT_ENUM(kDeveloperPrivate);
STATIC_ASSERT_ENUM(kDevtools);
STATIC_ASSERT_ENUM(kDns);
STATIC_ASSERT_ENUM(kDocumentScan);
STATIC_ASSERT_ENUM(kDownloads);
STATIC_ASSERT_ENUM(kDownloadsInternal);
STATIC_ASSERT_ENUM(kDownloadsOpen);
STATIC_ASSERT_ENUM(kDownloadsShelf);
STATIC_ASSERT_ENUM(kDeleted_EasyUnlockPrivate);
STATIC_ASSERT_ENUM(kEchoPrivate);
STATIC_ASSERT_ENUM(kDeleted_EmbeddedExtensionOptions);
STATIC_ASSERT_ENUM(kEnterprisePlatformKeys);
STATIC_ASSERT_ENUM(kEnterprisePlatformKeysPrivate);
STATIC_ASSERT_ENUM(kDeleted_ExperienceSamplingPrivate);
STATIC_ASSERT_ENUM(kExperimental);
STATIC_ASSERT_ENUM(kDeleted_ExtensionView);
STATIC_ASSERT_ENUM(kExternallyConnectableAllUrls);
STATIC_ASSERT_ENUM(kFeedbackPrivate);
STATIC_ASSERT_ENUM(kFileBrowserHandler);
STATIC_ASSERT_ENUM(kFileBrowserHandlerInternal);
STATIC_ASSERT_ENUM(kFileManagerPrivate);
STATIC_ASSERT_ENUM(kFileSystem);
STATIC_ASSERT_ENUM(kFileSystemDirectory);
STATIC_ASSERT_ENUM(kFileSystemProvider);
STATIC_ASSERT_ENUM(kFileSystemRequestFileSystem);
STATIC_ASSERT_ENUM(kFileSystemRetainEntries);
STATIC_ASSERT_ENUM(kFileSystemWrite);
STATIC_ASSERT_ENUM(kDeleted_FileSystemWriteDirectory);
STATIC_ASSERT_ENUM(kFirstRunPrivate);
STATIC_ASSERT_ENUM(kFontSettings);
STATIC_ASSERT_ENUM(kFullscreen);
STATIC_ASSERT_ENUM(kDeleted_GcdPrivate);
STATIC_ASSERT_ENUM(kGcm);
STATIC_ASSERT_ENUM(kGeolocation);
STATIC_ASSERT_ENUM(kHid);
STATIC_ASSERT_ENUM(kHistory);
STATIC_ASSERT_ENUM(kHomepage);
STATIC_ASSERT_ENUM(kHotwordPrivate);
STATIC_ASSERT_ENUM(kIdentity);
STATIC_ASSERT_ENUM(kIdentityEmail);
STATIC_ASSERT_ENUM(kIdentityPrivate);
STATIC_ASSERT_ENUM(kIdltest);
STATIC_ASSERT_ENUM(kIdle);
STATIC_ASSERT_ENUM(kImeWindowEnabled);
STATIC_ASSERT_ENUM(kDeleted_InlineInstallPrivate);
STATIC_ASSERT_ENUM(kInput);
STATIC_ASSERT_ENUM(kInputMethodPrivate);
STATIC_ASSERT_ENUM(kDeleted_InterceptAllKeys);
STATIC_ASSERT_ENUM(kLauncherSearchProvider);
STATIC_ASSERT_ENUM(kLocation);
STATIC_ASSERT_ENUM(kDeleted_LogPrivate);
STATIC_ASSERT_ENUM(kManagement);
STATIC_ASSERT_ENUM(kMediaGalleries);
STATIC_ASSERT_ENUM(kMediaPlayerPrivate);
STATIC_ASSERT_ENUM(kMediaRouterPrivate);
STATIC_ASSERT_ENUM(kMetricsPrivate);
STATIC_ASSERT_ENUM(kMDns);
STATIC_ASSERT_ENUM(kMusicManagerPrivate);
STATIC_ASSERT_ENUM(kNativeMessaging);
STATIC_ASSERT_ENUM(kDeleted_NetworkingConfig);
STATIC_ASSERT_ENUM(kNetworkingPrivate);
STATIC_ASSERT_ENUM(kDeleted_NotificationProvider);
STATIC_ASSERT_ENUM(kNotifications);
STATIC_ASSERT_ENUM(kOverrideEscFullscreen);
STATIC_ASSERT_ENUM(kPageCapture);
STATIC_ASSERT_ENUM(kPointerLock);
STATIC_ASSERT_ENUM(kPlatformKeys);
STATIC_ASSERT_ENUM(kDeleted_Plugin);
STATIC_ASSERT_ENUM(kPower);
STATIC_ASSERT_ENUM(kDeleted_PreferencesPrivate);
STATIC_ASSERT_ENUM(kDeleted_PrincipalsPrivate);
STATIC_ASSERT_ENUM(kPrinterProvider);
STATIC_ASSERT_ENUM(kPrivacy);
STATIC_ASSERT_ENUM(kProcesses);
STATIC_ASSERT_ENUM(kProxy);
STATIC_ASSERT_ENUM(kImageWriterPrivate);
STATIC_ASSERT_ENUM(kDeleted_ReadingListPrivate);
STATIC_ASSERT_ENUM(kRtcPrivate);
STATIC_ASSERT_ENUM(kSearchProvider);
STATIC_ASSERT_ENUM(kSearchEnginesPrivate);
STATIC_ASSERT_ENUM(kSerial);
STATIC_ASSERT_ENUM(kSessions);
STATIC_ASSERT_ENUM(kSettingsPrivate);
STATIC_ASSERT_ENUM(kSignedInDevices);
STATIC_ASSERT_ENUM(kSocket);
STATIC_ASSERT_ENUM(kStartupPages);
STATIC_ASSERT_ENUM(kStorage);
STATIC_ASSERT_ENUM(kDeleted_StreamsPrivate);
STATIC_ASSERT_ENUM(kSyncFileSystem);
STATIC_ASSERT_ENUM(kSystemPrivate);
STATIC_ASSERT_ENUM(kSystemDisplay);
STATIC_ASSERT_ENUM(kSystemStorage);
STATIC_ASSERT_ENUM(kTab);
STATIC_ASSERT_ENUM(kTabCapture);
STATIC_ASSERT_ENUM(kTabCaptureForTab);
STATIC_ASSERT_ENUM(kTerminalPrivate);
STATIC_ASSERT_ENUM(kTopSites);
STATIC_ASSERT_ENUM(kTts);
STATIC_ASSERT_ENUM(kTtsEngine);
STATIC_ASSERT_ENUM(kUnlimitedStorage);
STATIC_ASSERT_ENUM(kU2fDevices);
STATIC_ASSERT_ENUM(kUsb);
STATIC_ASSERT_ENUM(kUsbDevice);
STATIC_ASSERT_ENUM(kVideoCapture);
STATIC_ASSERT_ENUM(kVirtualKeyboardPrivate);
STATIC_ASSERT_ENUM(kVpnProvider);
STATIC_ASSERT_ENUM(kWallpaper);
STATIC_ASSERT_ENUM(kWallpaperPrivate);
STATIC_ASSERT_ENUM(kWebcamPrivate);
STATIC_ASSERT_ENUM(kDeleted_kWebConnectable);
STATIC_ASSERT_ENUM(kWebNavigation);
STATIC_ASSERT_ENUM(kWebRequest);
STATIC_ASSERT_ENUM(kWebRequestBlocking);
STATIC_ASSERT_ENUM(kWebrtcAudioPrivate);
STATIC_ASSERT_ENUM(kWebrtcDesktopCapturePrivate);
STATIC_ASSERT_ENUM(kWebrtcLoggingPrivate);
STATIC_ASSERT_ENUM(kWebstorePrivate);
STATIC_ASSERT_ENUM(kWebstoreWidgetPrivate);
STATIC_ASSERT_ENUM(kWebView);
STATIC_ASSERT_ENUM(kWindowShape);
STATIC_ASSERT_ENUM(kDeleted_ScreenlockPrivate);
STATIC_ASSERT_ENUM(kSystemCpu);
STATIC_ASSERT_ENUM(kSystemMemory);
STATIC_ASSERT_ENUM(kSystemNetwork);
STATIC_ASSERT_ENUM(kSystemInfoCpu);
STATIC_ASSERT_ENUM(kSystemInfoMemory);
STATIC_ASSERT_ENUM(kBluetooth);
STATIC_ASSERT_ENUM(kBluetoothDevices);
STATIC_ASSERT_ENUM(kFavicon);
STATIC_ASSERT_ENUM(kFullAccess);
STATIC_ASSERT_ENUM(kHostReadOnly);
STATIC_ASSERT_ENUM(kHostReadWrite);
STATIC_ASSERT_ENUM(kHostsAll);
STATIC_ASSERT_ENUM(kHostsAllReadOnly);
STATIC_ASSERT_ENUM(kMediaGalleriesAllGalleriesCopyTo);
STATIC_ASSERT_ENUM(kMediaGalleriesAllGalleriesDelete);
STATIC_ASSERT_ENUM(kMediaGalleriesAllGalleriesRead);
STATIC_ASSERT_ENUM(kNetworkState);
STATIC_ASSERT_ENUM(kDeleted_OverrideBookmarksUI);
STATIC_ASSERT_ENUM(kShouldWarnAllHosts);
STATIC_ASSERT_ENUM(kSocketAnyHost);
STATIC_ASSERT_ENUM(kSocketDomainHosts);
STATIC_ASSERT_ENUM(kSocketSpecificHosts);
STATIC_ASSERT_ENUM(kDeleted_UsbDeviceList);
STATIC_ASSERT_ENUM(kUsbDeviceUnknownProduct);
STATIC_ASSERT_ENUM(kUsbDeviceUnknownVendor);
STATIC_ASSERT_ENUM(kUsersPrivate);
STATIC_ASSERT_ENUM(kPasswordsPrivate);
STATIC_ASSERT_ENUM(kLanguageSettingsPrivate);
STATIC_ASSERT_ENUM(kEnterpriseDeviceAttributes);
STATIC_ASSERT_ENUM(kCertificateProvider);
STATIC_ASSERT_ENUM(kResourcesPrivate);
STATIC_ASSERT_ENUM(kDeleted_DisplaySource);
STATIC_ASSERT_ENUM(kClipboard);
STATIC_ASSERT_ENUM(kNetworkingOnc);
STATIC_ASSERT_ENUM(kVirtualKeyboard);
STATIC_ASSERT_ENUM(kNetworkingCastPrivate);
STATIC_ASSERT_ENUM(kMediaPerceptionPrivate);
STATIC_ASSERT_ENUM(kLockScreen);
STATIC_ASSERT_ENUM(kNewTabPageOverride);
STATIC_ASSERT_ENUM(kDeclarativeNetRequest);
STATIC_ASSERT_ENUM(kLockWindowFullscreenPrivate);
STATIC_ASSERT_ENUM(kWebrtcLoggingPrivateAudioDebug);
STATIC_ASSERT_ENUM(kEnterpriseReportingPrivate);
STATIC_ASSERT_ENUM(kCecPrivate);
STATIC_ASSERT_ENUM(kSafeBrowsingPrivate);
STATIC_ASSERT_ENUM(kFileSystemRequestDownloads);
STATIC_ASSERT_ENUM(kDeleted_SystemPowerSource);
STATIC_ASSERT_ENUM(kArcAppsPrivate);
STATIC_ASSERT_ENUM(kEnterpriseHardwarePlatform);
STATIC_ASSERT_ENUM(kLoginScreenUi);
STATIC_ASSERT_ENUM(kDeclarativeNetRequestFeedback);
STATIC_ASSERT_ENUM(kTransientBackground);
STATIC_ASSERT_ENUM(kLogin);
STATIC_ASSERT_ENUM(kLoginScreenStorage);
STATIC_ASSERT_ENUM(kLoginState);
STATIC_ASSERT_ENUM(kPrintingMetrics);
STATIC_ASSERT_ENUM(kPrinting);
STATIC_ASSERT_ENUM(kCrashReportPrivate);
STATIC_ASSERT_ENUM(kAutofillAssistantPrivate);
STATIC_ASSERT_ENUM(kEnterpriseNetworkingAttributes);
STATIC_ASSERT_ENUM(kSearch);
STATIC_ASSERT_ENUM(kTabGroups);
STATIC_ASSERT_ENUM(kScripting);

}  // namespace

namespace extensions {

APIPermission::APIPermission(const APIPermissionInfo* info)
  : info_(info) {
  DCHECK(info_);
}

APIPermission::~APIPermission() { }

APIPermission::ID APIPermission::id() const {
  return info()->id();
}

const char* APIPermission::name() const {
  return info()->name();
}

//
// APIPermissionInfo
//

APIPermissionInfo::APIPermissionInfo(const APIPermissionInfo::InitInfo& info)
    : name_(info.name),
      id_(info.id),
      flags_(info.flags),
      api_permission_constructor_(info.constructor) {}

APIPermissionInfo::~APIPermissionInfo() { }

std::unique_ptr<APIPermission> APIPermissionInfo::CreateAPIPermission() const {
  if (api_permission_constructor_)
    return api_permission_constructor_(this);
  return std::make_unique<SimpleAPIPermission>(this);
}

}  // namespace extensions
