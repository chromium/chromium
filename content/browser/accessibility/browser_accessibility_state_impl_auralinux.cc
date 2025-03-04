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

#include "base/metrics/histogram_macros.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/public/browser/browser_thread.h"
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
    re2::RE2 orca_regex(R"((^|/)(usr/)?bin/orca(\s|$))");
    if (re2::RE2::PartialMatch(cmdline, orca_regex)) {
      return true;  // Orca was found
    }
  }
  return false;  // Orca was not found
}

}  // namespace

class BrowserAccessibilityStateImplAuralinux
    : public BrowserAccessibilityStateImpl {
 public:
  BrowserAccessibilityStateImplAuralinux() = default;

 protected:
  void UpdateUniqueUserHistograms() override;
  void UpdateKnownAssistiveTechSlow() override;
  BrowserAccessibilityState::AssistiveTech ActiveKnownAssistiveTech() override;

 private:
  bool is_orca_active_ = false;
};

void BrowserAccessibilityStateImplAuralinux::UpdateKnownAssistiveTechSlow() {
  // NOTE: this method is run from another thread to reduce jank, since
  // there's no guarantee these system calls will return quickly. Code that
  // needs to run in the UI thread can be run in
  // UpdateHistogramsOnUIThread instead.
  std::unique_ptr<DIR, decltype(&CloseDir)> proc_dir(opendir("/proc"),
                                                     &CloseDir);
  if (proc_dir == nullptr) {
    LOG(ERROR) << "Error opening /proc directory.";
    return;
  }

  is_orca_active_ = false;
  raw_ptr<dirent> entry;
  while (!is_orca_active_ && (entry = readdir(proc_dir.get())) != nullptr) {
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
            is_orca_active_ = CheckCmdlineForOrca(cmdline_all);
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

  UMA_HISTOGRAM_BOOLEAN("Accessibility.Linux.Orca", is_orca_active_);
  static auto* ax_orca_crash_key = base::debug::AllocateCrashKeyString(
      "ax_orca", base::debug::CrashKeySize::Size32);
  if (is_orca_active_) {
    base::debug::SetCrashKeyString(ax_orca_crash_key, "true");
  } else {
    base::debug::ClearCrashKeyString(ax_orca_crash_key);
  }

  awaiting_known_assistive_tech_computation_ = false;
}

void BrowserAccessibilityStateImplAuralinux::UpdateUniqueUserHistograms() {
  BrowserAccessibilityStateImpl::UpdateUniqueUserHistograms();

  UMA_HISTOGRAM_BOOLEAN("Accessibility.Linux.Orca.EveryReport",
                        is_orca_active_);
}

BrowserAccessibilityState::AssistiveTech
BrowserAccessibilityStateImplAuralinux::ActiveKnownAssistiveTech() {
  if (awaiting_known_assistive_tech_computation_) {
    return kUnknown;
  }
  return is_orca_active_ ? kOrca : kNone;
}

// static
std::unique_ptr<BrowserAccessibilityStateImpl>
BrowserAccessibilityStateImpl::Create() {
  return std::make_unique<BrowserAccessibilityStateImplAuralinux>();
}

}  // namespace content
