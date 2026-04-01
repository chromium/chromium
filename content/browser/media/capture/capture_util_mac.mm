// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/capture_util_mac.h"

#include <AppKit/AppKit.h>
#include <libproc.h>

#include <optional>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "content/browser/media/capture/screen_capture_kit_device_mac.h"
#include "content/public/browser/web_contents.h"
#include "media/capture/video/video_capture_device.h"
#include "third_party/webrtc/modules/desktop_capture/mac/window_list_utils.h"

namespace {

pid_t (*g_get_parent_pid_for_testing)(pid_t) = nullptr;
std::optional<std::string> (*g_get_bundle_id_for_process_for_testing)(pid_t) =
    nullptr;
pid_t (*g_get_window_owner_pid_for_testing)(content::DesktopMediaID::Id) =
    nullptr;

pid_t GetParentPid(pid_t pid) {
  if (g_get_parent_pid_for_testing) {
    return g_get_parent_pid_for_testing(pid);  // IN-TEST
  }
  struct proc_bsdinfo info;
  if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info)) <= 0) {
    return 0;  // Failed to get info
  }
  return static_cast<pid_t>(info.pbi_ppid);
}

std::optional<std::string> GetBundleIdForProcess(pid_t pid) {
  if (g_get_bundle_id_for_process_for_testing) {
    return g_get_bundle_id_for_process_for_testing(pid);  // IN-TEST
  }
  NSRunningApplication* app =
      [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
  if (!app || !app.bundleIdentifier) {
    return std::nullopt;
  }
  return std::make_optional(base::SysNSStringToUTF8(app.bundleIdentifier));
}

pid_t GetWindowOwnerPid(content::DesktopMediaID::Id window_id) {
  if (g_get_window_owner_pid_for_testing) {
    return g_get_window_owner_pid_for_testing(window_id);  // IN-TEST
  }
  return webrtc::GetWindowOwnerPid(window_id);
}

}  // namespace

namespace content {

std::unique_ptr<media::VideoCaptureDevice> CreateScreenCaptureKitDeviceMac(
    const DesktopMediaID& source,
    bool is_native_picker,
    std::unique_ptr<PipScreenCaptureCoordinatorProxy>
        pip_screen_capture_coordinator_proxy) {
  // Although ScreenCaptureKit is available in 12.3 there were some bugs that
  // were not fixed until 13.2.
  if (@available(macOS 13.2, *)) {
    return CreateScreenCaptureKitDeviceMac(
        source, is_native_picker, /*filter=*/nullptr,
        /*callback=*/base::DoNothing(),
        std::move(pip_screen_capture_coordinator_proxy));
  } else {
    return nullptr;
  }
}

std::optional<DesktopMediaID::Id> GetNativeWindowIdMac(
    WebContents& web_contents) {
  gfx::NativeView native_view = web_contents.GetNativeView();
  NSView* ns_view = native_view.GetNativeNSView();
  if (!ns_view) {
    return std::nullopt;
  }

  NSWindow* ns_window = [ns_view window];
  if (!ns_window) {
    return std::nullopt;
  }

  int64_t window_number = [ns_window windowNumber];
  return window_number > 0 ? std::make_optional(
                                 static_cast<DesktopMediaID::Id>(window_number))
                           : std::nullopt;
}

std::optional<std::string> GetMainBundleIdForNativeWindowId(
    DesktopMediaID::Id window_id) {
  pid_t window_owner_pid = GetWindowOwnerPid(window_id);
  // GetWindowOwnerPid() returns 0 if the owner is not found.
  if (!window_owner_pid) {
    return std::nullopt;
  }

  std::optional<std::string> current_bundle_id =
      GetBundleIdForProcess(window_owner_pid);

  if (!current_bundle_id) {
    return std::nullopt;
  }

  pid_t current_pid = window_owner_pid;
  while (true) {
    pid_t parent_pid = GetParentPid(current_pid);
    // Stop if we hit system root (GetParentPid returns 1) or if GetParentPid
    // fails (returns 0).
    if (parent_pid <= 1) {
      break;
    }
    std::optional<std::string> parent_bundle_id =
        GetBundleIdForProcess(parent_pid);

    // Stop if the parent has no bundle identifier.
    if (!parent_bundle_id) {
      break;
    }

    // Stop if parent has a different bundle identifier.
    if (*parent_bundle_id != *current_bundle_id &&
        !base::StartsWith(*current_bundle_id,
                          base::StrCat({*parent_bundle_id, "."}),
                          base::CompareCase::SENSITIVE)) {
      break;
    }

    // The parent matches; move up the tree and keep climbing.
    current_pid = parent_pid;
    current_bundle_id = std::move(parent_bundle_id);
  }

  return current_bundle_id;
}

void SetGetParentPidForTesting(  // IN-TEST
    GetParentPidForTestingCallback func) {
  g_get_parent_pid_for_testing = func;
}
void SetGetBundleIdForProcessForTesting(  // IN-TEST
    GetBundleIdForProcessForTestingCallback func) {
  g_get_bundle_id_for_process_for_testing = func;
}
void SetGetWindowOwnerPidForTesting(  // IN-TEST
    GetWindowOwnerPidForTestingCallback func) {
  g_get_window_owner_pid_for_testing = func;
}

}  // namespace content
