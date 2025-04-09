// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include <windows.h>  // Must be in front of other Windows header files.

#include <stddef.h>

#include <memory>

#include "base/check_deref.h"
#include "base/containers/heap_array.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/win/registry.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"

namespace content {

namespace {

const wchar_t kNarratorRegistryKey[] = L"Software\\Microsoft\\Narrator\\NoRoam";
const wchar_t kNarratorRunningStateValueName[] = L"RunningState";

static constexpr uint32_t kJaws = 0x01 << 0;
static constexpr uint32_t kNvda = 0x01 << 1;
static constexpr uint32_t kNarrator = 0x01 << 2;
static constexpr uint32_t kSupernova = 0x01 << 3;
static constexpr uint32_t kZdsr = 0x01 << 4;
static constexpr uint32_t kZoomtext = 0x01 << 5;
static constexpr uint32_t kUia = 0x01 << 6;  // API support, not a specific AT.
static constexpr uint32_t kStickyKeys = 0x01 << 7;

// Returns a bitfield indicating the set of assistive techs that are active.
uint32_t DiscoverAssistiveTech() {
  uint32_t discovered_ats = 0;

  // NOTE: this method is run from another thread to reduce jank, since
  // there's no guarantee these system calls will return quickly.

  STICKYKEYS sticky_keys = {.cbSize = sizeof(STICKYKEYS)};
  SystemParametersInfo(SPI_GETSTICKYKEYS, 0, &sticky_keys, 0);
  if (sticky_keys.dwFlags & SKF_STICKYKEYSON) {
    discovered_ats |= kStickyKeys;
  }

  // Narrator detection. Narrator is not injected in process so it needs to be
  // detected in a different way.
  DWORD narrator_value = 0;
  if (base::win::RegKey(HKEY_CURRENT_USER, kNarratorRegistryKey,
                        KEY_QUERY_VALUE)
              .ReadValueDW(kNarratorRunningStateValueName, &narrator_value) ==
          ERROR_SUCCESS &&
      narrator_value) {
    discovered_ats |= kNarrator;
  }

  std::vector<HMODULE> snapshot;
  if (!base::win::GetLoadedModulesSnapshot(::GetCurrentProcess(), &snapshot)) {
    return discovered_ats;
  }
  TCHAR filename[MAX_PATH];
  for (HMODULE module : snapshot) {
    auto name_length =
        ::GetModuleFileName(module, filename, std::size(filename));
    if (name_length == 0 || name_length >= std::size(filename)) {
      continue;
    }
    std::string module_name(base::FilePath(filename).BaseName().AsUTF8Unsafe());
    if (base::EqualsCaseInsensitiveASCII(module_name, "fsdomsrv.dll")) {
      discovered_ats |= kJaws;
    }
    if (base::EqualsCaseInsensitiveASCII(module_name,
                                         "vbufbackend_gecko_ia2.dll") ||
        base::EqualsCaseInsensitiveASCII(module_name, "nvdahelperremote.dll")) {
      discovered_ats |= kNvda;
    }
    if (base::EqualsCaseInsensitiveASCII(module_name, "dolwinhk.dll")) {
      discovered_ats |= kSupernova;
    }
    if (base::EqualsCaseInsensitiveASCII(module_name, "outhelper.dll") ||
        base::EqualsCaseInsensitiveASCII(module_name, "outhelper_x64.dll")) {
      discovered_ats |= kZdsr;  // Zhengdu screen reader.
    }
    if (base::EqualsCaseInsensitiveASCII(module_name, "zslhook.dll") ||
        base::EqualsCaseInsensitiveASCII(module_name, "zslhook64.dll")) {
      discovered_ats |= kZoomtext;
    }
    if (base::EqualsCaseInsensitiveASCII(module_name, "uiautomation.dll") ||
        base::EqualsCaseInsensitiveASCII(module_name, "uiautomationcore.dll")) {
      discovered_ats |= kUia;
    }
  }

  return discovered_ats;
}

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
                    ui::AXMode::kExtendedProperties);
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
    BrowserAccessibilityStateImpl::GetInstance()
        ->NotifyWebContentsPreferencesChanged();
  }
}

}  // namespace

class BrowserAccessibilityStateImplWin : public BrowserAccessibilityStateImpl {
 public:
  BrowserAccessibilityStateImplWin();

 protected:
  void RefreshAssistiveTech() override;
  ui::AXPlatform::ProductStrings GetProductStrings() override;
  void OnUiaProviderRequested(bool uia_provider_enabled) override;

 private:
  void OnDiscoveredAssistiveTech(uint32_t discovered_ats);

  std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;

  // The presence of an AssistiveTech is currently being recomputed.
  // Will be updated via DiscoverAssistiveTech().
  bool awaiting_known_assistive_tech_computation_ = false;
};

BrowserAccessibilityStateImplWin::BrowserAccessibilityStateImplWin() {
  ui::GetWinAccessibilityAPIUsageObserverList().AddObserver(
      new WindowsAccessibilityEnabler());

  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    singleton_hwnd_observer_ = std::make_unique<gfx::SingletonHwndObserver>(
        base::BindRepeating(&OnWndProc));
  }
}

void BrowserAccessibilityStateImplWin::RefreshAssistiveTech() {
  if (!awaiting_known_assistive_tech_computation_) {
    awaiting_known_assistive_tech_computation_ = true;
    // Using base::Unretained() instead of a weak pointer as the lifetime of
    // this is tied to BrowserMainLoop.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&DiscoverAssistiveTech),
        base::BindOnce(
            &BrowserAccessibilityStateImplWin::OnDiscoveredAssistiveTech,
            base::Unretained(this)));
  }
}

void BrowserAccessibilityStateImplWin::OnDiscoveredAssistiveTech(
    uint32_t discovered_ats) {
  awaiting_known_assistive_tech_computation_ = false;

  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinJAWS", (discovered_ats & kJaws) != 0);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinNarrator",
                        (discovered_ats & kNarrator) != 0);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinNVDA", (discovered_ats & kNvda) != 0);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinSupernova",
                        (discovered_ats & kZdsr) != 0);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinZDSR",
                        (discovered_ats & kZoomtext) != 0);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinZoomText",
                        (discovered_ats & kJaws) != 0);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinAPIs.UIAutomation",
                        (discovered_ats & kUia) != 0);
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinStickyKeys",
                        (discovered_ats & kStickyKeys) != 0);

  static auto* ax_jaws_crash_key = base::debug::AllocateCrashKeyString(
      "ax_jaws", base::debug::CrashKeySize::Size32);
  static auto* ax_narrator_crash_key = base::debug::AllocateCrashKeyString(
      "ax_narrator", base::debug::CrashKeySize::Size32);
  static auto* ax_nvda_crash_key = base::debug::AllocateCrashKeyString(
      "ax_nvda", base::debug::CrashKeySize::Size32);
  static auto* ax_supernova_crash_key = base::debug::AllocateCrashKeyString(
      "ax_supernova", base::debug::CrashKeySize::Size32);
  static auto* ax_zdsr_crash_key = base::debug::AllocateCrashKeyString(
      "ax_zdsr", base::debug::CrashKeySize::Size32);
  static auto* ax_zoomtext_crash_key = base::debug::AllocateCrashKeyString(
      "ax_zoomtext", base::debug::CrashKeySize::Size32);
  static auto* ax_uia_crash_key = base::debug::AllocateCrashKeyString(
      "ax_ui_automation", base::debug::CrashKeySize::Size32);

  // API support library, not an actual AT.
  if (discovered_ats & kUia) {
    base::debug::SetCrashKeyString(ax_uia_crash_key, "true");
  } else {
    base::debug::ClearCrashKeyString(ax_uia_crash_key);
  }

  // More than one type of assistive tech can be running at the same time.
  // Will prefer to report screen reader over other types of assistive tech,
  // because screen readers have the strongest effect on the user experience.
  ui::AssistiveTech most_important_assistive_tech = ui::AssistiveTech::kNone;

  if (discovered_ats & kZoomtext) {
    base::debug::SetCrashKeyString(ax_zoomtext_crash_key, "true");
    most_important_assistive_tech = ui::AssistiveTech::kZoomText;
  } else {
    base::debug::ClearCrashKeyString(ax_zoomtext_crash_key);
  }

  if (discovered_ats & kJaws) {
    base::debug::SetCrashKeyString(ax_jaws_crash_key, "true");
    most_important_assistive_tech = ui::AssistiveTech::kJaws;
  } else {
    base::debug::ClearCrashKeyString(ax_jaws_crash_key);
  }

  if (discovered_ats & kNarrator) {
    most_important_assistive_tech = ui::AssistiveTech::kNarrator;
    base::debug::SetCrashKeyString(ax_narrator_crash_key, "true");
  } else {
    base::debug::ClearCrashKeyString(ax_narrator_crash_key);
  }

  if (discovered_ats & kNvda) {
    most_important_assistive_tech = ui::AssistiveTech::kNvda;
    base::debug::SetCrashKeyString(ax_nvda_crash_key, "true");
  } else {
    base::debug::ClearCrashKeyString(ax_nvda_crash_key);
  }

  if (discovered_ats & kSupernova) {
    base::debug::SetCrashKeyString(ax_supernova_crash_key, "true");
    most_important_assistive_tech = ui::AssistiveTech::kSupernova;
  } else {
    base::debug::ClearCrashKeyString(ax_supernova_crash_key);
  }

  if (discovered_ats & kZdsr) {
    base::debug::SetCrashKeyString(ax_zdsr_crash_key, "true");
    most_important_assistive_tech = ui::AssistiveTech::kZdsr;
  } else {
    base::debug::ClearCrashKeyString(ax_zdsr_crash_key);
  }

  OnAssistiveTechFound(most_important_assistive_tech);
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
