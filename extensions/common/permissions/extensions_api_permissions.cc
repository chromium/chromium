// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/extensions_api_permissions.h"

#include <stddef.h>

#include <memory>

#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/socket_permission.h"
#include "extensions/common/permissions/usb_device_permission.h"

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
    {APIPermission::kAlarms, "alarms",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kAlphaEnabled, "app.window.alpha",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kAlwaysOnTopWindows, "app.window.alwaysOnTop",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kAppView, "appview",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kAudio, "audio",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kAudioCapture, "audioCapture",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermission::kBluetoothPrivate, "bluetoothPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kCecPrivate, "cecPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kClipboard, "clipboard"},
    {APIPermission::kClipboardRead, "clipboardRead",
     APIPermissionInfo::kFlagSupportsContentCapabilities},
    {APIPermission::kClipboardWrite, "clipboardWrite",
     APIPermissionInfo::kFlagSupportsContentCapabilities |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kCrashReportPrivate, "crashReportPrivate"},
    {APIPermission::kDeclarativeWebRequest, "declarativeWebRequest"},
    {APIPermission::kDiagnostics, "diagnostics",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kDns, "dns"},
    {APIPermission::kExternallyConnectableAllUrls,
     "externally_connectable.all_urls",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFeedbackPrivate, "feedbackPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kFullscreen, "app.window.fullscreen",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},

    // The permission string for "fileSystem" is only shown when
    // "write" or "directory" is present. Read-only access is only
    // granted after the user has been shown a file or directory
    // chooser dialog and selected a file or directory. Selecting
    // the file or directory is considered consent to read it.
    {APIPermission::kFileSystem, "fileSystem",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFileSystemDirectory, "fileSystem.directory",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFileSystemRequestFileSystem,
     "fileSystem.requestFileSystem"},
    {APIPermission::kFileSystemRetainEntries, "fileSystem.retainEntries",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kFileSystemWrite, "fileSystem.write",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},

    {APIPermission::kHid, "hid"},
    {APIPermission::kImeWindowEnabled, "app.window.ime"},
    {APIPermission::kOverrideEscFullscreen, "app.window.fullscreen.overrideEsc",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kIdle, "idle",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kLockScreen, "lockScreen"},
    {APIPermission::kLockWindowFullscreenPrivate, "lockWindowFullscreenPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kLogin, "login"},
    {APIPermission::kLoginScreenStorage, "loginScreenStorage"},
    {APIPermission::kLoginScreenUi, "loginScreenUi"},
    {APIPermission::kLoginState, "loginState",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kMediaPerceptionPrivate, "mediaPerceptionPrivate"},
    {APIPermission::kMetricsPrivate, "metricsPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kNativeMessaging, "nativeMessaging",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kNetworkingOnc, "networking.onc"},
    {APIPermission::kNetworkingPrivate, "networkingPrivate",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kNewTabPageOverride, "newTabPageOverride",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermission::kPower, "power",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kPrinterProvider, "printerProvider",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kPrinting, "printing",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermission::kPrintingMetrics, "printingMetrics",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermission::kSerial, "serial",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSocket, "socket",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning,
     &CreateAPIPermission<SocketPermission>},
    {APIPermission::kStorage, "storage",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSystemCpu, "system.cpu",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSystemMemory, "system.memory",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSystemNetwork, "system.network",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSystemDisplay, "system.display",
     APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kSystemStorage, "system.storage"},
    {APIPermission::kU2fDevices, "u2fDevices"},
    {APIPermission::kUnlimitedStorage, "unlimitedStorage",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagSupportsContentCapabilities |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kUsb, "usb", APIPermissionInfo::kFlagNone},
    {APIPermission::kUsbDevice, "usbDevices",
     extensions::APIPermissionInfo::kFlagNone,
     &CreateAPIPermission<UsbDevicePermission>},
    {APIPermission::kVideoCapture, "videoCapture",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermission::kVirtualKeyboard, "virtualKeyboard"},
    {APIPermission::kVpnProvider, "vpnProvider",
     APIPermissionInfo::kFlagCannotBeOptional |
         APIPermissionInfo::kFlagDoesNotRequireManagedSessionFullLoginWarning},
    {APIPermission::kWebRequest, "webRequest"},
    {APIPermission::kWebRequestBlocking, "webRequestBlocking"},
    {APIPermission::kDeclarativeNetRequest,
     declarative_net_request::kAPIPermission,
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kWebView, "webview",
     APIPermissionInfo::kFlagCannotBeOptional},
    {APIPermission::kWindowShape, "app.window.shape"},
    {APIPermission::kFileSystemRequestDownloads, "fileSystem.requestDownloads"},
    {APIPermission::kDeclarativeNetRequestFeedback,
     declarative_net_request::kFeedbackAPIPermission,
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
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
