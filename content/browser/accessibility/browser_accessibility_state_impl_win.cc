// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include <windows.h>  // Must be in front of other Windows header files.

#include <psapi.h>
#include <stddef.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"

namespace content {

namespace {

// Enables accessibility based on three possible clues that indicate
// accessibility API usage.
//
// TODO(dmazzoni): Rename IAccessible2UsageObserver to something more general.
class WindowsAccessibilityEnabler : public ui::IAccessible2UsageObserver {
 public:
  WindowsAccessibilityEnabler() {}

 private:
  // IAccessible2UsageObserver
  void OnIAccessible2Used() override {
    // When IAccessible2 APIs have been used elsewhere in the codebase,
    // enable basic web accessibility support. (Full screen reader support is
    // detected later when specific more advanced APIs are accessed.)
    BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
        ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents);
  }

  void OnScreenReaderHoneyPotQueried() override {
    // We used to trust this as a signal that a screen reader is running,
    // but it's been abused. Now only enable accessibility if we also
    // detect a call to get_accName.
    if (screen_reader_honeypot_queried_)
      return;
    screen_reader_honeypot_queried_ = true;
    if (acc_name_called_) {
      BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
          ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents);
    }
  }

  void OnAccNameCalled() override {
    // See OnScreenReaderHoneyPotQueried, above.
    if (acc_name_called_)
      return;
    acc_name_called_ = true;
    if (screen_reader_honeypot_queried_) {
      BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
          ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents);
    }
  }

  bool screen_reader_honeypot_queried_ = false;
  bool acc_name_called_ = false;
};

}  // namespace

void BrowserAccessibilityStateImpl::PlatformInitialize() {
  ui::GetIAccessible2UsageObserverList().AddObserver(
      new WindowsAccessibilityEnabler());
}

void BrowserAccessibilityStateImpl::UpdatePlatformSpecificHistograms() {
  // NOTE: this method is run from the file thread to reduce jank, since
  // there's no guarantee these system calls will return quickly. Be careful
  // not to add any code that isn't safe to run from a non-main thread!

  AUDIODESCRIPTION audio_description = {0};
  audio_description.cbSize = sizeof(AUDIODESCRIPTION);
  SystemParametersInfo(SPI_GETAUDIODESCRIPTION, 0, &audio_description, 0);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinAudioDescription",
                        !!audio_description.Enabled);

  BOOL win_screen_reader = FALSE;
  SystemParametersInfo(SPI_GETSCREENREADER, 0, &win_screen_reader, 0);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinScreenReader",
                        !!win_screen_reader);

  STICKYKEYS sticky_keys = {0};
  sticky_keys.cbSize = sizeof(STICKYKEYS);
  SystemParametersInfo(SPI_GETSTICKYKEYS, 0, &sticky_keys, 0);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinStickyKeys",
                        0 != (sticky_keys.dwFlags & SKF_STICKYKEYSON));

  // Get the file paths of all DLLs loaded.
  HANDLE process = GetCurrentProcess();
  HMODULE* modules = NULL;
  DWORD bytes_required;
  if (!EnumProcessModules(process, modules, 0, &bytes_required))
    return;

  std::unique_ptr<char[]> buffer(new char[bytes_required]);
  modules = reinterpret_cast<HMODULE*>(buffer.get());
  DWORD ignore;
  if (!EnumProcessModules(process, modules, bytes_required, &ignore))
    return;

  // Look for DLLs of assistive technology known to work with Chrome.
  bool jaws = false;
  bool nvda = false;
  bool satogo = false;
  bool zoomtext = false;
  size_t module_count = bytes_required / sizeof(HMODULE);
  for (size_t i = 0; i < module_count; i++) {
    TCHAR filename[MAX_PATH];
    GetModuleFileName(modules[i], filename, arraysize(filename));
    base::string16 module_name(base::FilePath(filename).BaseName().value());
    if (base::LowerCaseEqualsASCII(module_name, "fsdomsrv.dll"))
      jaws = true;
    if (base::LowerCaseEqualsASCII(module_name, "vbufbackend_gecko_ia2.dll"))
      nvda = true;
    if (base::LowerCaseEqualsASCII(module_name, "stsaw32.dll"))
      satogo = true;
    if (base::LowerCaseEqualsASCII(module_name, "zslhook.dll"))
      zoomtext = true;
  }

  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinJAWS", jaws);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinNVDA", nvda);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinSAToGo", satogo);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinZoomText", zoomtext);
}

}  // namespace content
