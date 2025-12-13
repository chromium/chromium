// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include <windows.h>  // Must be in front of other Windows header files.

#include <stddef.h>

#include <memory>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/containers/heap_array.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/win/registry.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace content {

namespace {

// Killswitch to turn off this feature remotely in case it affects ATs in a way
// we didn't expect. This is temporary.
// TODO(crbug.com/407891291): Remove this feature flag in Chrome 139.
BASE_FEATURE(kDisableUiaProviderWhenJawsIsRunning,
             base::FEATURE_ENABLED_BY_DEFAULT);

const wchar_t kNarratorRegistryKey[] = L"Software\\Microsoft\\Narrator\\NoRoam";
const wchar_t kWinMagnifierRegistryKey[] =
    L"Software\\Microsoft\\ScreenMagnifier";
const wchar_t kWinATRunningStateValueName[] = L"RunningState";

enum class AccessibilityTarget {
  kStickyKeys,
  kUia,
  kJaws,
  kNarrator,
  kNvda,
  kWinMagnifier,
  kSupernova,
  kZoomText,
  kZdsr,
};

struct ModuleVersion {
  uint16_t major = 0, minor = 0, build = 0, revision = 0;

  bool IsLowerThan(const ModuleVersion& other) const {
    if (major != other.major) {
      return major < other.major;
    }
    if (minor != other.minor) {
      return minor < other.minor;
    }
    if (build != other.build) {
      return build < other.build;
    }
    return revision < other.revision;
  }

  std::string ToString() const {
    return base::StringPrintf("%u.%u.%u.%u", major, minor, build, revision);
  }
};

struct AssistiveTechInfo {
  AccessibilityTarget tech;
  std::optional<ModuleVersion> version;
};

std::optional<ModuleVersion> GetModuleVersion(const std::wstring& filename) {
  DWORD dummy = 0;
  DWORD size = ::GetFileVersionInfoSizeW(filename.c_str(), &dummy);
  if (size == 0) {
    return std::nullopt;
  }

  std::vector<BYTE> buffer(size);
  if (!::GetFileVersionInfoW(filename.c_str(), dummy, size, buffer.data())) {
    return std::nullopt;
  }

  VS_FIXEDFILEINFO* ffi = nullptr;
  UINT len = 0;
  if (::VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<void**>(&ffi),
                       &len) &&
      len != 0 && ffi->dwSignature == VS_FFI_SIGNATURE) {
    uint16_t major = HIWORD(ffi->dwProductVersionMS);
    uint16_t minor = LOWORD(ffi->dwProductVersionMS);
    uint16_t build = HIWORD(ffi->dwProductVersionLS);
    uint16_t revision = LOWORD(ffi->dwProductVersionLS);
    return ModuleVersion{major, minor, build, revision};
  }
  return std::nullopt;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(JawsMajorVersion)
enum class JawsMajorVersion {
  kLegacy = 0,
  k2020 = 1,
  k2021 = 2,
  k2022 = 3,
  k2023 = 4,
  k2024 = 5,
  k2025 = 6,
  k2026 = 7,
  k2027 = 8,
  k2028 = 9,
  k2029 = 10,
  k2030 = 11,
  k2031 = 12,
  k2032 = 13,
  k2033 = 14,
  k2034 = 15,
  k2035 = 16,
  k2036 = 17,
  k2037 = 18,
  k2038 = 19,
  k2039 = 20,
  k2040 = 21,
  kPost2040 = 22,
  kMaxValue = kPost2040,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:JAWSMajorVersion)

JawsMajorVersion MapModuleVersionToJaws(const ModuleVersion& version) {
  constexpr uint16_t kFirstKnownVersion = 2020;
  constexpr uint16_t kLastKnownVersion = 2040;
  if (version.major > kLastKnownVersion) {
    return JawsMajorVersion::kPost2040;
  }
  if (version.major >= kFirstKnownVersion &&
      version.major <= kLastKnownVersion) {
    return static_cast<JawsMajorVersion>(version.major -
                                         (kFirstKnownVersion - 1));
  }
  return JawsMajorVersion::kLegacy;
}

// Older versions of JAWS are known to not work well with text fields when we
// expose the native UIA provider. Disable it when we detect an older version
// version of JAWS. JAWS fixed the issue in versions:
//   * 2022.2402.1+
//   * 2023.2402.1+
//   * 2024.2312.99+
//   * 2025+
bool IsJawsCompatibleWithUIA(const ModuleVersion& version) {
  return !(version.IsLowerThan(ModuleVersion{2022, 0, 0, 0}) ||
           (version.major == 2022 &&
            version.IsLowerThan(ModuleVersion{2022, 2402, 1, 0})) ||
           (version.major == 2023 &&
            version.IsLowerThan(ModuleVersion{2023, 2402, 1, 0})) ||
           (version.major == 2024 &&
            version.IsLowerThan(ModuleVersion{2024, 2312, 99, 0})));
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(NvdaMajorVersion)
enum class NvdaMajorVersion {
  kLegacy = 0,
  k2020 = 1,
  k2021 = 2,
  k2022 = 3,
  k2023 = 4,
  k2024 = 5,
  k2025 = 6,
  k2026 = 7,
  k2027 = 8,
  k2028 = 9,
  k2029 = 10,
  k2030 = 11,
  k2031 = 12,
  k2032 = 13,
  k2033 = 14,
  k2034 = 15,
  k2035 = 16,
  k2036 = 17,
  k2037 = 18,
  k2038 = 19,
  k2039 = 20,
  k2040 = 21,
  kPost2040 = 22,
  kMaxValue = kPost2040,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:NVDAMajorVersion)

NvdaMajorVersion MapModuleVersionToNvda(const ModuleVersion& version) {
  constexpr uint16_t kFirstKnownVersion = 2020;
  constexpr uint16_t kLastKnownVersion = 2040;
  if (version.major > kLastKnownVersion) {
    return NvdaMajorVersion::kPost2040;
  }
  if (version.major >= kFirstKnownVersion &&
      version.major <= kLastKnownVersion) {
    return static_cast<NvdaMajorVersion>(version.major -
                                         (kFirstKnownVersion - 1));
  }
  return NvdaMajorVersion::kLegacy;
}

// Returns a vector of all Assistive Technologies that are currently running,
// and their versions if available. We return a vector instead of a map
// because it's technically possible to have multiple versions of the same
// AT running at the same time.
std::vector<AssistiveTechInfo> DiscoverAssistiveTech() {
  std::vector<AssistiveTechInfo> discovered_ats;

  // NOTE: this method is run from another thread to reduce jank, since
  // there's no guarantee these system calls will return quickly.

  STICKYKEYS sticky_keys = {.cbSize = sizeof(STICKYKEYS)};
  SystemParametersInfo(SPI_GETSTICKYKEYS, 0, &sticky_keys, 0);
  if (sticky_keys.dwFlags & SKF_STICKYKEYSON) {
    discovered_ats.push_back({AccessibilityTarget::kStickyKeys, std::nullopt});
  }

  // Narrator detection. Narrator is not injected in process so it needs to be
  // detected in a different way.
  DWORD narrator_value = 0;
  if (base::win::RegKey(HKEY_CURRENT_USER, kNarratorRegistryKey,
                        KEY_QUERY_VALUE)
              .ReadValueDW(kWinATRunningStateValueName, &narrator_value) ==
          ERROR_SUCCESS &&
      narrator_value) {
    discovered_ats.push_back({AccessibilityTarget::kNarrator, std::nullopt});
  }

  // Windows magnifier detection.
  DWORD windows_magnifier_value = 0;
  if (base::win::RegKey(HKEY_CURRENT_USER, kWinMagnifierRegistryKey,
                        KEY_QUERY_VALUE)
              .ReadValueDW(kWinATRunningStateValueName,
                           &windows_magnifier_value) == ERROR_SUCCESS &&
      windows_magnifier_value) {
    discovered_ats.push_back(
        {AccessibilityTarget::kWinMagnifier, std::nullopt});
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
      discovered_ats.push_back(
          {AccessibilityTarget::kJaws, GetModuleVersion(filename)});
    }
    if (base::EqualsCaseInsensitiveASCII(module_name,
                                         "vbufbackend_gecko_ia2.dll") ||
        base::EqualsCaseInsensitiveASCII(module_name, "nvdahelperremote.dll")) {
      discovered_ats.push_back(
          {AccessibilityTarget::kNvda, GetModuleVersion(filename)});
    }
    if (base::EqualsCaseInsensitiveASCII(module_name, "dolwinhk.dll")) {
      discovered_ats.push_back(
          {AccessibilityTarget::kSupernova, GetModuleVersion(filename)});
    }
    if (base::EqualsCaseInsensitiveASCII(module_name, "outhelper.dll") ||
        base::EqualsCaseInsensitiveASCII(module_name, "outhelper_x64.dll")) {
      discovered_ats.push_back(
          {AccessibilityTarget::kZdsr, GetModuleVersion(filename)});
    }
    if (base::EqualsCaseInsensitiveASCII(module_name, "zslhook.dll") ||
        base::EqualsCaseInsensitiveASCII(module_name, "zslhook64.dll")) {
      discovered_ats.push_back(
          {AccessibilityTarget::kZoomText, GetModuleVersion(filename)});
    }
    if (base::EqualsCaseInsensitiveASCII(module_name, "uiautomation.dll") ||
        base::EqualsCaseInsensitiveASCII(module_name, "uiautomationcore.dll")) {
      discovered_ats.push_back(
          {AccessibilityTarget::kUia, GetModuleVersion(filename)});
    }
  }

  return discovered_ats;
}

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
  void RefreshAssistiveTechIfNecessary(ui::AXMode new_mode) override;
  ui::AXPlatform::ProductStrings GetProductStrings() override;

 private:
  void OnDiscoveredAssistiveTech(
      const std::vector<AssistiveTechInfo>& discovered_ats);

  base::CallbackListSubscription hwnd_subscription_;

  // A ScopedAccessibilityMode that holds AXMode::kScreenReader when
  // an active screen reader has been detected.
  std::unique_ptr<ScopedAccessibilityMode> screen_reader_mode_;

  // The presence of an AssistiveTech is currently being recomputed.
  // Will be updated via DiscoverAssistiveTech().
  bool awaiting_known_assistive_tech_computation_ = false;
};

BrowserAccessibilityStateImplWin::BrowserAccessibilityStateImplWin() {
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    hwnd_subscription_ = gfx::SingletonHwnd::GetInstance()->RegisterCallback(
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

void BrowserAccessibilityStateImplWin::RefreshAssistiveTechIfNecessary(
    ui::AXMode new_mode) {
  bool was_screen_reader_active = ax_platform().IsScreenReaderActive();
  bool has_screen_reader_mode = new_mode.has_mode(ui::AXMode::kScreenReader);
  if (was_screen_reader_active != has_screen_reader_mode) {
    OnAssistiveTechFound(has_screen_reader_mode
                             ? ui::AssistiveTech::kGenericScreenReader
                             : ui::AssistiveTech::kNone);
    return;
  }

  // An expensive check is required to determine which type of assistive tech is
  // in use. Make this check only when `kExtendedProperties` is added or removed
  // from the process-wide mode flags and no previous assistive tech has been
  // discovered (in the former case) or one had been discovered (in the latter
  // case). `kScreenReader` will be added/removed from the process-wide mode
  // flags on completion and `OnAssistiveTechFound()` will be called with the
  // results of the check.
  bool has_extended_properties =
      new_mode.has_mode(ui::AXMode::kExtendedProperties);
  if (was_screen_reader_active != has_extended_properties) {
    // Perform expensive assistive tech detection.
    RefreshAssistiveTech();
  }
}

void BrowserAccessibilityStateImplWin::OnDiscoveredAssistiveTech(
    const std::vector<AssistiveTechInfo>& at_infos) {
  awaiting_known_assistive_tech_computation_ = false;

  if (ActiveAssistiveTech() == ui::AssistiveTech::kGenericScreenReader) {
    // A test has overridden the screen reader state manually.
    // In such cases, we don't want to alter it.
    return;
  }

  // Older versions of JAWS are known to not work well with text fields when we
  // expose the native UIA provider. Disable it when we detect an older version
  // version of JAWS. JAWS fixed the issue in versions:
  //   * 2022.2402.1+
  //   * 2023.2402.1+
  //   * 2024.2312.99+
  //   * 2025+
  if (base::FeatureList::IsEnabled(kDisableUiaProviderWhenJawsIsRunning) &&
      ui::AXPlatform::GetInstance().IsUiaProviderEnabled()) {
    for (const auto& info : at_infos) {
      if (info.tech == AccessibilityTarget::kJaws && info.version.has_value()) {
        if (!IsJawsCompatibleWithUIA(*info.version)) {
          ui::AXPlatform::GetInstance().DisableActiveUiaProvider();
          break;
        }
      }
    }
  }

  // Helper lambda to check for a specific AT.
  auto HasTarget = [&at_infos](AccessibilityTarget target) -> bool {
    for (const auto& info : at_infos) {
      if (info.tech == target) {
        return true;
      }
    }
    return false;
  };

  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinJAWS",
                        HasTarget(AccessibilityTarget::kJaws));
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinNarrator",
                        HasTarget(AccessibilityTarget::kNarrator));
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinNVDA",
                        HasTarget(AccessibilityTarget::kNvda));
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinSupernova",
                        HasTarget(AccessibilityTarget::kSupernova));
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinMagnifier",
                        HasTarget(AccessibilityTarget::kWinMagnifier));
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinZoomText",
                        HasTarget(AccessibilityTarget::kZoomText));
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinAPIs.UIAutomation",
                        HasTarget(AccessibilityTarget::kUia));
  UMA_HISTOGRAM_BOOLEAN("Accessibility.WinStickyKeys",
                        HasTarget(AccessibilityTarget::kStickyKeys));

  static auto* ax_jaws_crash_key = base::debug::AllocateCrashKeyString(
      "ax_jaws", base::debug::CrashKeySize::Size32);
  static auto* ax_narrator_crash_key = base::debug::AllocateCrashKeyString(
      "ax_narrator", base::debug::CrashKeySize::Size32);
  static auto* ax_win_magnifier_crash_key = base::debug::AllocateCrashKeyString(
      "ax_win_magnifier", base::debug::CrashKeySize::Size32);
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
  if (HasTarget(AccessibilityTarget::kUia)) {
    base::debug::SetCrashKeyString(ax_uia_crash_key, "true");
  } else {
    base::debug::ClearCrashKeyString(ax_uia_crash_key);
  }

  // More than one type of assistive tech can be running at the same time.
  // Will prefer to report screen reader over other types of assistive tech,
  // because screen readers have the strongest effect on the user experience.
  ui::AssistiveTech most_important_assistive_tech = ui::AssistiveTech::kNone;

  if (HasTarget(AccessibilityTarget::kWinMagnifier)) {
    base::debug::SetCrashKeyString(ax_narrator_crash_key, "true");
    most_important_assistive_tech = ui::AssistiveTech::kWinMagnifier;
  } else {
    base::debug::ClearCrashKeyString(ax_win_magnifier_crash_key);
  }

  if (HasTarget(AccessibilityTarget::kZoomText)) {
    base::debug::SetCrashKeyString(ax_zoomtext_crash_key, "true");
    most_important_assistive_tech = ui::AssistiveTech::kZoomText;
  } else {
    base::debug::ClearCrashKeyString(ax_zoomtext_crash_key);
  }

  if (HasTarget(AccessibilityTarget::kJaws)) {
    base::debug::SetCrashKeyString(ax_jaws_crash_key, "true");
    most_important_assistive_tech = ui::AssistiveTech::kJaws;
  } else {
    base::debug::ClearCrashKeyString(ax_jaws_crash_key);
  }

  if (HasTarget(AccessibilityTarget::kNarrator)) {
    base::debug::SetCrashKeyString(ax_narrator_crash_key, "true");
    most_important_assistive_tech = ui::AssistiveTech::kNarrator;
  } else {
    base::debug::ClearCrashKeyString(ax_narrator_crash_key);
  }

  if (HasTarget(AccessibilityTarget::kNvda)) {
    base::debug::SetCrashKeyString(ax_nvda_crash_key, "true");
    most_important_assistive_tech = ui::AssistiveTech::kNvda;
  } else {
    base::debug::ClearCrashKeyString(ax_nvda_crash_key);
  }

  if (HasTarget(AccessibilityTarget::kSupernova)) {
    base::debug::SetCrashKeyString(ax_supernova_crash_key, "true");
    most_important_assistive_tech = ui::AssistiveTech::kSupernova;
  } else {
    base::debug::ClearCrashKeyString(ax_supernova_crash_key);
  }

  if (HasTarget(AccessibilityTarget::kZdsr)) {
    base::debug::SetCrashKeyString(ax_zdsr_crash_key, "true");
    most_important_assistive_tech = ui::AssistiveTech::kZdsr;
  } else {
    base::debug::ClearCrashKeyString(ax_zdsr_crash_key);
  }

  // Histograms for the JAWS and NVDA versions.
  for (const auto& info : at_infos) {
    if (info.tech == AccessibilityTarget::kJaws && info.version) {
      UMA_HISTOGRAM_BOOLEAN("Accessibility.WinJAWSCompatibleWithUIA",
                            IsJawsCompatibleWithUIA(*info.version));
      JawsMajorVersion jaws_version = MapModuleVersionToJaws(*info.version);
      base::UmaHistogramEnumeration("Accessibility.WinJAWSVersion",
                                    jaws_version);
      continue;
    }
    if (info.tech == AccessibilityTarget::kNvda && info.version) {
      NvdaMajorVersion nvda_version = MapModuleVersionToNvda(*info.version);
      base::UmaHistogramEnumeration("Accessibility.WinNVDAVersion",
                                    nvda_version);
      continue;
    }
  }

  // Save the current assistive tech before toggling AXModes, so
  // that RefreshAssistiveTechIfNecessary() is a noop.
  OnAssistiveTechFound(most_important_assistive_tech);

  // Add kScreenReader mode flag for products with screen reader features, which
  // includes some magnifiers with light screen reader features (e.g. heading
  // navigation).
  if (ui::IsScreenReader(most_important_assistive_tech)) {
    if (!screen_reader_mode_) {
      screen_reader_mode_ = CreateScopedModeForProcess(
          ui::kAXModeComplete | ui::AXMode::kScreenReader);
    }
  } else {
    screen_reader_mode_.reset();
  }
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

// static
std::unique_ptr<BrowserAccessibilityStateImpl>
BrowserAccessibilityStateImpl::Create() {
  return std::make_unique<BrowserAccessibilityStateImplWin>();
}

}  // namespace content
