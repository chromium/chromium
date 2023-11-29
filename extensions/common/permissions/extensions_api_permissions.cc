// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/extensions_api_permissions.h"

#include <stddef.h>

#include <memory>

#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/socket_permission.h"
#include "extensions/common/permissions/usb_device_permission.h"

using extensions::mojom::APIPermissionID;

namespace extensions {
namespace api_permissions {

namespace {

template <typename T>
std::unique_ptr<APIPermission> CreateAPIPermission(
    const APIPermissionInfo* permission) {
  return std::make_unique<T>(permission);
}

// WARNING: If you are modifying a permission message in this list, be sure to
// add the corresponding permission message rule to
// ChromePermissionMessageRule::GetAllRules as well.
constexpr APIPermissionInfo::InitInfo permissions_to_register[] = {
    {APIPermissionID::kActiveTab, "activeTab"},
    {APIPermissionID::kAlarms, "alarms",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kAlphaEnabled, "app.window.alpha",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kAlwaysOnTopWindows, "app.window.alwaysOnTop",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kAppView, "appview",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kAudio, "audio",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kAudioCapture, "audioCapture",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermissionID::kBluetoothPrivate, "bluetoothPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kCecPrivate, "cecPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kClipboard, "clipboard"},
    {APIPermissionID::kClipboardRead, "clipboardRead",
     APIPermissionInfo::kFlagSupportsContentCapabilities},
    {APIPermissionID::kClipboardWrite, "clipboardWrite",
     APIPermissionInfo::kFlagSupportsContentCapabilities |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kCrashReportPrivate, "crashReportPrivate"},
    {APIPermissionID::kDeclarativeNetRequest,
     declarative_net_request::kDeclarativeNetRequestPermission,
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kDeclarativeNetRequestFeedback,
     declarative_net_request::kFeedbackAPIPermission,
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermissionID::kDeclarativeNetRequestWithHostAccess,
     "declarativeNetRequestWithHostAccess"},
    {APIPermissionID::kDeclarativeWebRequest, "declarativeWebRequest"},
    {APIPermissionID::kDiagnostics, "diagnostics",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kDns, "dns"},
    {APIPermissionID::kDeprecated_ExternallyConnectableAllUrls,
     "externally_connectable.all_urls",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kFeedbackPrivate, "feedbackPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kFullscreen, "app.window.fullscreen",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},

    // The permission string for "fileSystem" is only shown when
    // "write" or "directory" is present. Read-only access is only
    // granted after the user has been shown a file or directory
    // chooser dialog and selected a file or directory. Selecting
    // the file or directory is considered consent to read it.
    {APIPermissionID::kFileSystem, "fileSystem",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kFileSystemDirectory, "fileSystem.directory",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kFileSystemRequestFileSystem,
     "fileSystem.requestFileSystem"},
    {APIPermissionID::kFileSystemRetainEntries, "fileSystem.retainEntries",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kFileSystemWrite, "fileSystem.write",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kHid, "hid"},
    {APIPermissionID::kImeWindowEnabled, "app.window.ime"},
    {APIPermissionID::kOverrideEscFullscreen,
     "app.window.fullscreen.overrideEsc",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kIdle, "idle",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kLockScreen, "lockScreen"},
    {APIPermissionID::kLockWindowFullscreenPrivate,
     "lockWindowFullscreenPrivate", APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kLogin, "login"},
    {APIPermissionID::kLoginScreenStorage, "loginScreenStorage"},
    {APIPermissionID::kLoginScreenUi, "loginScreenUi"},
    {APIPermissionID::kLoginState, "loginState",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kMediaPerceptionPrivate, "mediaPerceptionPrivate"},
    {APIPermissionID::kMetricsPrivate, "metricsPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kNativeMessaging, "nativeMessaging",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kNetworkingOnc, "networking.onc"},
    {APIPermissionID::kNetworkingPrivate, "networkingPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kNewTabPageOverride, "newTabPageOverride",
     APIPermissionInfo::kFlagInternal |
         APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermissionID::kOdfsConfigPrivate, "odfsConfigPrivate",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kOffscreen, "offscreen",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kPower, "power",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kPrinterProvider, "printerProvider",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kPrinting, "printing",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermissionID::kPrintingMetrics, "printingMetrics",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermissionID::kSerial, "serial",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kSharedStoragePrivate, "sharedStoragePrivate"},
    {APIPermissionID::kSocket, "socket",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning,
     &CreateAPIPermission<SocketPermission>},
    {APIPermissionID::kSpeechRecognitionPrivate, "speechRecognitionPrivate",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kStorage, "storage",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kSystemCpu, "system.cpu",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kSystemLog, "systemLog",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kSystemMemory, "system.memory",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kSystemNetwork, "system.network",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kSystemDisplay, "system.display",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kSystemStorage, "system.storage"},
    {APIPermissionID::kU2fDevices, "u2fDevices"},
    {APIPermissionID::kUnlimitedStorage, "unlimitedStorage",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagSupportsContentCapabilities |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kUserScripts, "userScripts",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kUsb, "usb", APIPermissionInfo::kFlagNone},
    {APIPermissionID::kUsbDevice, "usbDevices",
     extensions::APIPermissionInfo::kFlagNone,
     &CreateAPIPermission<UsbDevicePermission>},
    {APIPermissionID::kVideoCapture, "videoCapture",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermissionID::kVirtualKeyboard, "virtualKeyboard"},
    {APIPermissionID::kVpnProvider, "vpnProvider",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermissionID::kWebRequest, "webRequest"},
    {APIPermissionID::kWebRequestAuthProvider, "webRequestAuthProvider"},
    {APIPermissionID::kWebRequestBlocking, "webRequestBlocking"},
    {APIPermissionID::kWebView, "webview",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermissionID::kWindowShape, "app.window.shape"},
    {APIPermissionID::kWmDesksPrivate, "wmDesksPrivate"},
};

}  // namespace

base::span<const APIPermissionInfo::InitInfo> GetPermissionInfos() {
  return base::make_span(permissions_to_register);
}

base::span<const Alias> GetPermissionAliases() {
  // In alias constructor, first value is the alias name; second value is the
  // real name. See also alias.h.
  static constexpr Alias aliases[] = {
      Alias("alwaysOnTopWindows", "app.window.alwaysOnTop"),
      Alias("fullscreen", "app.window.fullscreen"),
      Alias("overrideEscFullscreen", "app.window.fullscreen.overrideEsc"),
      Alias("unlimited_storage", "unlimitedStorage")};

  return base::make_span(aliases);
}

}  // namespace api_permissions
}  // namespace extensions
