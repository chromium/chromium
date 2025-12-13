// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "base/debug/crash_logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "third_party/re2/src/re2/re2.h"

namespace content {

namespace {

// Custom deleter function for DIR* that calls closedir
void CloseDir(DIR* dir) {
  if (dir) {
    closedir(dir);
  }
}

bool CheckCmdlineForOrca(const std::string& cmdline_all) {
  std::string cmdline;
  std::stringstream ss(cmdline_all);
  while (std::getline(ss, cmdline, '\0')) {
    if (cmdline == "orca") {
      return true;  // Orca was found
    }
  }
  return false;  // Orca was not found
}

// Returns true if Orca is active.
bool DiscoverOrca() {
  // NOTE: this method is run from another thread to reduce jank, since
  // there's no guarantee these system calls will return quickly.
  std::unique_ptr<DIR, decltype(&CloseDir)> proc_dir(opendir("/proc"),
                                                     &CloseDir);
  if (proc_dir == nullptr) {
    LOG(ERROR) << "Error opening /proc directory.";
    return false;
  }

  bool is_orca_active = false;
  raw_ptr<dirent> entry;
  while (!is_orca_active && (entry = readdir(proc_dir.get())) != nullptr) {
    if (isdigit(entry->d_name[0])) {
      std::string pidStr(entry->d_name);

      struct stat stat_buf;
      std::string stat_path = "/proc/" + pidStr;
      if (stat(stat_path.c_str(), &stat_buf) == 0) {
        if (stat_buf.st_uid == getuid()) {
          std::ifstream cmdline_file("/proc/" + pidStr + "/cmdline");
          if (cmdline_file.is_open()) {
            std::stringstream buffer;
            buffer << cmdline_file.rdbuf();
            std::string cmdline_all = buffer.str();
            is_orca_active = CheckCmdlineForOrca(cmdline_all);
            cmdline_file.close();
          } else {
            DVLOG(1) << "Error opening cmdline for pid: " << pidStr;
          }
        }
      } else {
        DVLOG(1) << "Error with stat for pid: " << pidStr;
      }
    }
  }

  return is_orca_active;
}

}  // namespace

class BrowserAccessibilityStateImplAuralinux
    : public BrowserAccessibilityStateImpl {
 public:
  BrowserAccessibilityStateImplAuralinux() = default;

  // BrowserAccessibilityStateImpl:
  void RefreshAssistiveTech() override;
  void RefreshAssistiveTechIfNecessary(ui::AXMode new_mode) override;

 private:
  void OnDiscoveredOrca(bool is_orca_active);

  // A ScopedAccessibilityMode that holds AXMode::kScreenReader when
  // an active screen reader has been detected.
  std::unique_ptr<ScopedAccessibilityMode> screen_reader_mode_;

  // The presence of an AssistiveTech is currently being recomputed.
  // Will be updated via DiscoverOrca().
  bool awaiting_known_assistive_tech_computation_ = false;
};

void BrowserAccessibilityStateImplAuralinux::RefreshAssistiveTech() {
  if (!awaiting_known_assistive_tech_computation_) {
    awaiting_known_assistive_tech_computation_ = true;
    // Using base::Unretained() instead of a weak pointer as the lifetime of
    // this is tied to BrowserMainLoop.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&DiscoverOrca),
        base::BindOnce(
            &BrowserAccessibilityStateImplAuralinux::OnDiscoveredOrca,
            base::Unretained(this)));
  }
}

void BrowserAccessibilityStateImplAuralinux::RefreshAssistiveTechIfNecessary(
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

void BrowserAccessibilityStateImplAuralinux::OnDiscoveredOrca(
    bool is_orca_active) {
  awaiting_known_assistive_tech_computation_ = false;

  if (ActiveAssistiveTech() == ui::AssistiveTech::kGenericScreenReader) {
    // A test has overridden the screen reader state manually.
    // In such cases, we don't want to alter it.
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("Accessibility.Linux.Orca", is_orca_active);
  static auto* ax_orca_crash_key = base::debug::AllocateCrashKeyString(
      "ax_orca", base::debug::CrashKeySize::Size32);
  // Save the current assistive tech before toggling AXModes, so
  // that RefreshAssistiveTechIfNecessary() is a noop.
  if (is_orca_active) {
    base::debug::SetCrashKeyString(ax_orca_crash_key, "true");
    OnAssistiveTechFound(ui::AssistiveTech::kOrca);
    if (!screen_reader_mode_) {
      screen_reader_mode_ = CreateScopedModeForProcess(
          ui::kAXModeComplete | ui::AXMode::kScreenReader);
    }
  } else {
    base::debug::ClearCrashKeyString(ax_orca_crash_key);
    OnAssistiveTechFound(ui::AssistiveTech::kNone);
    screen_reader_mode_.reset();
  }
}

// static
std::unique_ptr<BrowserAccessibilityStateImpl>
BrowserAccessibilityStateImpl::Create() {
  return std::make_unique<BrowserAccessibilityStateImplAuralinux>();
}

}  // namespace content
