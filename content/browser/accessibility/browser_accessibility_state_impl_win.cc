// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include <windows.h>  // Must be in front of other Windows header files.

#include <psapi.h>
#include <stddef.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
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
  void OnIAccessible2Used() override {
    // When IAccessible2 APIs have been used elsewhere in the codebase,
    // enable basic web accessibility support. (Full screen reader support is
    // detected later when specific more advanced APIs are accessed.)
    BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
        ui::kAXModeBasic);
    BrowserAccessibilityStateImpl::GetInstance()->OnAccessibilityApiUsage();
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
          ui::kAXModeBasic);
    }
    BrowserAccessibilityStateImpl::GetInstance()->OnAccessibilityApiUsage();
  }

  void OnAccNameCalled() override {
    // See OnScreenReaderHoneyPotQueried, above.
    if (acc_name_called_)
      return;
    acc_name_called_ = true;
    if (screen_reader_honeypot_queried_) {
      BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
          ui::kAXModeBasic);
    }
    BrowserAccessibilityStateImpl::GetInstance()->OnAccessibilityApiUsage();
  }

  void OnBasicUIAutomationUsed() override {
    AddAXModeForUIA(ui::AXMode::kNativeAPIs);
  }

  void OnAdvancedUIAutomationUsed() override {
    AddAXModeForUIA(ui::AXMode::kWebContents);
  }

  void OnUIAutomationIdRequested() override {
    // TODO(janewman): Currently, UIA_AutomationIdPropertyId currently uses
    // GetAuthorUniqueId. This implementation requires html to be enabled, we
    // should avoid needing all of kHTML by either modifying what we return for
    // this property or serializing the author supplied ID attribute separately.
    // Separating out the id is being tracked by crbug 703277
    AddAXModeForUIA(ui::AXMode::kHTML);
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
    DCHECK(::switches::IsExperimentalAccessibilityPlatformUIAEnabled());

    // Firing a UIA event can cause UIA to call back into our APIs, don't
    // consider this to be usage.
    if (firing_uia_events_)
      return;

    // UI Automation insulates providers from knowing about the client(s) asking
    // for information. When IsSelectiveUIAEnablement is Enabled, we turn on
    // various parts of accessibility depending on what APIs have been called.
    if (!features::IsSelectiveUIAEnablementEnabled())
      mode = ui::kAXModeComplete;
    BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
        mode);
    BrowserAccessibilityStateImpl::GetInstance()->OnAccessibilityApiUsage();
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
  ~BrowserAccessibilityStateImplWin() override {}

 protected:
  void UpdateHistogramsOnOtherThread() override;
  void UpdateUniqueUserHistograms() override;

 private:
  std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;
};

BrowserAccessibilityStateImplWin::BrowserAccessibilityStateImplWin() {
  ui::GetWinAccessibilityAPIUsageObserverList().AddObserver(
      new WindowsAccessibilityEnabler());

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
  size_t module_count = bytes_required / sizeof(HMODULE);
  bool satogo = false;  // Very few users -- do not need uniques
  for (size_t i = 0; i < module_count; i++) {
    TCHAR filename[MAX_PATH];
    GetModuleFileName(modules[i], filename, std::size(filename));
    std::string module_name(base::FilePath(filename).BaseName().AsUTF8Unsafe());
    if (base::EqualsCaseInsensitiveASCII(module_name, "fsdomsrv.dll"))
      g_jaws = true;
    if (base::EqualsCaseInsensitiveASCII(module_name,
                                         "vbufbackend_gecko_ia2.dll") ||
        base::EqualsCaseInsensitiveASCII(module_name, "nvdahelperremote.dll"))
      g_nvda = true;
    if (base::EqualsCaseInsensitiveASCII(module_name, "stsaw32.dll"))
      satogo = true;
    if (base::EqualsCaseInsensitiveASCII(module_name, "dolwinhk.dll"))
      g_supernova = true;
    if (base::EqualsCaseInsensitiveASCII(module_name, "zslhook.dll") ||
        base::EqualsCaseInsensitiveASCII(module_name, "zslhook64.dll"))
      g_zoomtext = true;
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

//
// BrowserAccessibilityStateImpl::GetInstance implementation that constructs
// this class instead of the base class.
//

// static
BrowserAccessibilityStateImpl* BrowserAccessibilityStateImpl::GetInstance() {
  static base::NoDestructor<BrowserAccessibilityStateImplWin> instance;
  return &*instance;
}

}  // namespace content
