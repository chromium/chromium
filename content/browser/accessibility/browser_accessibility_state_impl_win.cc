// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include <windows.h>  // Must be in front of other Windows header files.

#include <psapi.h>
#include <stddef.h>

#include <memory>

#include "base/check_deref.h"
#include "base/containers/heap_array.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"

namespace content {

namespace {

static bool g_jaws = false;
static bool g_nvda = false;
static bool g_supernova = false;
static bool g_zoomtext = false;

// Enables accessibility based on clues that indicate accessibility API usage.
class WindowsAccessibilityEnabler
    : public ui::WinAccessibilityAPIUsageObserver {
 public:
  WindowsAccessibilityEnabler() {}

 private:
  // WinAccessibilityAPIUsageObserver
  void OnMSAAUsed() override {
    // When only basic MSAA functionality is used, just enable kNativeAPIs.
    // Enabling kNativeAPIs gives little perf impact, but allows these APIs to
    // interact with the BrowserAccessibilityManager allowing ATs to be able at
    // least find the document without using any advanced APIs.
    BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
        ui::AXMode::kNativeAPIs);
  }

  void OnBasicIAccessible2Used() override {
    BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
        ui::AXMode::kNativeAPIs);
  }

  void OnAdvancedIAccessible2Used() override {
    // When IAccessible2 APIs have been used elsewhere in the codebase,
    // enable basic web accessibility support. (Full screen reader support is
    // detected later when specific more advanced APIs are accessed.)
    BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
        ui::kAXModeBasic);
  }

  void OnScreenReaderHoneyPotQueried() override {
    // We used to trust this as a signal that a screen reader is running,
    // but it's been abused. Now only enable accessibility if we also
    // detect a call to get_accName.
    if (screen_reader_honeypot_queried_) {
      return;
    }
    screen_reader_honeypot_queried_ = true;
    if (acc_name_called_) {
      BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
          ui::kAXModeBasic);
    }
  }

  void OnAccNameCalled() override {
    // See OnScreenReaderHoneyPotQueried, above.
    if (acc_name_called_) {
      return;
    }
    acc_name_called_ = true;
    if (screen_reader_honeypot_queried_) {
      BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
          ui::kAXModeBasic);
    }
  }

  void OnBasicUIAutomationUsed() override {
    AddAXModeForUIA(ui::AXMode::kNativeAPIs);
  }

  void OnAdvancedUIAutomationUsed() override {
    AddAXModeForUIA(ui::AXMode::kWebContents);
  }

  void OnProbableUIAutomationScreenReaderDetected() override {
    // Same as kAXModeComplete but without kHTML as it is not needed for UIA.
    AddAXModeForUIA(ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents |
                    ui::AXMode::kScreenReader);
  }

  void OnTextPatternRequested() override {
    AddAXModeForUIA(ui::AXMode::kInlineTextBoxes);
  }

  void AddAXModeForUIA(ui::AXMode mode) {
    DCHECK(::ui::AXPlatform::GetInstance().IsUiaProviderEnabled());

    // Firing a UIA event can cause UIA to call back into our APIs, don't
    // consider this to be usage.
    if (firing_uia_events_) {
      return;
    }

    // UI Automation insulates providers from knowing about the client(s) asking
    // for information. When IsSelectiveUIAEnablement is Enabled, we turn on
    // various parts of accessibility depending on what APIs have been called.
    if (!features::IsSelectiveUIAEnablementEnabled()) {
      mode = ui::kAXModeComplete;
    }
    BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
        mode);
  }

  void StartFiringUIAEvents() override { firing_uia_events_ = true; }

  void EndFiringUIAEvents() override { firing_uia_events_ = false; }

  // This should be set to true while we are firing uia events. Firing UIA
  // events causes UIA to call back into our APIs, this should not be considered
  // usage.
  bool firing_uia_events_ = false;
  bool screen_reader_honeypot_queried_ = false;
  bool acc_name_called_ = false;
};

void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (message == WM_SETTINGCHANGE && wparam == SPI_SETCLIENTAREAANIMATION) {
    gfx::Animation::UpdatePrefersReducedMotion();
    for (WebContentsImpl* wc : WebContentsImpl::GetAllWebContents()) {
      wc->OnWebPreferencesChanged();
    }
  }
}

}  // namespace

class BrowserAccessibilityStateImplWin : public BrowserAccessibilityStateImpl {
 public:
  BrowserAccessibilityStateImplWin();

 protected:
  void InitBackgroundTasks() override;
  void UpdateHistogramsOnOtherThread() override;
  void UpdateUniqueUserHistograms() override;
  ui::AXPlatform::ProductStrings GetProductStrings() override;
  void OnUiaProviderRequested(bool uia_provider_enabled) override;

 private:
  std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;
};

BrowserAccessibilityStateImplWin::BrowserAccessibilityStateImplWin() {
  ui::GetWinAccessibilityAPIUsageObserverList().AddObserver(
      new WindowsAccessibilityEnabler());
}

void BrowserAccessibilityStateImplWin::InitBackgroundTasks() {
  BrowserAccessibilityStateImpl::InitBackgroundTasks();

  singleton_hwnd_observer_ = std::make_unique<gfx::SingletonHwndObserver>(
      base::BindRepeating(&OnWndProc));
}

void BrowserAccessibilityStateImplWin::UpdateHistogramsOnOtherThread() {
  BrowserAccessibilityStateImpl::UpdateHistogramsOnOtherThread();

  // NOTE: this method is run from another thread to reduce jank, since
  // there's no guarantee these system calls will return quickly. Code that
  // needs to run in the UI thread can be run in
  // UpdateHistogramsOnUIThread instead.

  // Better all-encompassing screen reader metric.
  // See also specific screen reader metrics below, e.g. WinJAWS, WinNVDA.
  ui::AXMode mode =
      BrowserAccessibilityStateImpl::GetInstance()->GetAccessibilityMode();
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinScreenReader2",
                        mode.has_mode(ui::AXMode::kScreenReader));

  STICKYKEYS sticky_keys = {0};
  sticky_keys.cbSize = sizeof(STICKYKEYS);
  SystemParametersInfo(SPI_GETSTICKYKEYS, 0, &sticky_keys, 0);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinStickyKeys",
                        0 != (sticky_keys.dwFlags & SKF_STICKYKEYSON));

  // Get the file paths of all DLLs loaded.
  HANDLE process = GetCurrentProcess();
  HMODULE* modules = nullptr;
  DWORD bytes_required;
  if (!EnumProcessModules(process, modules, 0, &bytes_required)) {
    return;
  }

  auto buffer = base::HeapArray<uint8_t>::WithSize(bytes_required);
  modules = reinterpret_cast<HMODULE*>(buffer.data());
  DWORD ignore;
  if (!EnumProcessModules(process, modules, bytes_required, &ignore)) {
    return;
  }

  // Look for DLLs of assistive technology known to work with Chrome.
  size_t module_count = bytes_required / sizeof(HMODULE);
  bool satogo = false;  // Very few users -- do not need uniques
  for (size_t i = 0; i < module_count; i++) {
    TCHAR filename[MAX_PATH];
    GetModuleFileName(modules[i], filename, std::size(filename));
    std::string module_name(base::FilePath(filename).BaseName().AsUTF8Unsafe());
    if (base::EqualsCaseInsensitiveASCII(module_name, "fsdomsrv.dll")) {
      static auto* ax_jaws_crash_key = base::debug::AllocateCrashKeyString(
          "ax_jaws", base::debug::CrashKeySize::Size32);
      base::debug::SetCrashKeyString(ax_jaws_crash_key, "true");
      g_jaws = true;
    }
    if (base::EqualsCaseInsensitiveASCII(module_name,
                                         "vbufbackend_gecko_ia2.dll") ||
        base::EqualsCaseInsensitiveASCII(module_name, "nvdahelperremote.dll")) {
      static auto* ax_nvda_crash_key = base::debug::AllocateCrashKeyString(
          "ax_nvda", base::debug::CrashKeySize::Size32);
      base::debug::SetCrashKeyString(ax_nvda_crash_key, "true");
      g_nvda = true;
    }
    if (base::EqualsCaseInsensitiveASCII(module_name, "stsaw32.dll")) {
      static auto* ax_satogo_crash_key = base::debug::AllocateCrashKeyString(
          "ax_satogo", base::debug::CrashKeySize::Size32);
      base::debug::SetCrashKeyString(ax_satogo_crash_key, "true");
      satogo = true;
    }
    if (base::EqualsCaseInsensitiveASCII(module_name, "dolwinhk.dll")) {
      static auto* ax_supernova_crash_key = base::debug::AllocateCrashKeyString(
          "ax_supernova", base::debug::CrashKeySize::Size32);
      base::debug::SetCrashKeyString(ax_supernova_crash_key, "true");
      g_supernova = true;
    }
    if (base::EqualsCaseInsensitiveASCII(module_name, "zslhook.dll") ||
        base::EqualsCaseInsensitiveASCII(module_name, "zslhook64.dll")) {
      static auto* ax_zoomtext_crash_key = base::debug::AllocateCrashKeyString(
          "ax_zoomtext", base::debug::CrashKeySize::Size32);
      base::debug::SetCrashKeyString(ax_zoomtext_crash_key, "true");
      g_zoomtext = true;
    }
  }

  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinJAWS", g_jaws);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinNVDA", g_nvda);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinSAToGo", satogo);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinSupernova", g_supernova);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinZoomText", g_zoomtext);
}

void BrowserAccessibilityStateImplWin::UpdateUniqueUserHistograms() {
  BrowserAccessibilityStateImpl::UpdateUniqueUserHistograms();

  ui::AXMode mode = GetAccessibilityMode();
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinScreenReader2.EveryReport",
                        mode.has_mode(ui::AXMode::kScreenReader));
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinJAWS.EveryReport", g_jaws);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinNVDA.EveryReport", g_nvda);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinSupernova.EveryReport", g_supernova);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinZoomText.EveryReport", g_zoomtext);
}

ui::AXPlatform::ProductStrings
BrowserAccessibilityStateImplWin::GetProductStrings() {
  ContentClient& content_client = CHECK_DEREF(content::GetContentClient());
  // GetProduct() returns a string like "Chrome/aa.bb.cc.dd", split out
  // the part before and after the "/".
  std::vector<std::string> product_components = base::SplitString(
      CHECK_DEREF(CHECK_DEREF(content::GetContentClient()).browser())
          .GetProduct(),
      "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (product_components.size() != 2) {
    return {{}, {}, CHECK_DEREF(content_client.browser()).GetUserAgent()};
  }
  return {product_components[0], product_components[1],
          CHECK_DEREF(content_client.browser()).GetUserAgent()};
}

void BrowserAccessibilityStateImplWin::OnUiaProviderRequested(
    bool uia_provider_enabled) {
  CHECK_DEREF(CHECK_DEREF(GetContentClient()).browser())
      .OnUiaProviderRequested(uia_provider_enabled);
}

// static
std::unique_ptr<BrowserAccessibilityStateImpl>
BrowserAccessibilityStateImpl::Create() {
  return std::make_unique<BrowserAccessibilityStateImplWin>();
}

}  // namespace content
