// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/synthetic_crash_report_util.h"

#include <stdlib.h>

#include "base/files/memory_mapped_file.h"
#include "base/ios/device_util.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Appends |config| vector with key value pair, respecting max value length.
void AppendConfig(std::vector<std::string>& config,
                  std::string key,
                  std::string value) {
  const size_t kMaxValueLen = 255;
  if (value.size() <= kMaxValueLen) {
    config.push_back(key);
    config.push_back(base::NumberToString(value.size()));
    config.push_back(value);
    return;
  }

  // Recursively split the value into chunks.
  int key_index = 1;
  size_t val_offset = 0;
  for (; val_offset < value.size(); val_offset += kMaxValueLen, key_index++) {
    AppendConfig(config, base::StringPrintf("%s__%d", key.c_str(), key_index),
                 value.substr(val_offset, kMaxValueLen));
  }
}

}  // namespace

void CreateSyntheticCrashReportForUte(
    const base::FilePath& path,
    const std::string& breakpad_product_display,
    const std::string& breakpad_product,
    const std::string& breakpad_version,
    const std::string& breakpad_url) {
  std::vector<std::string> config;

  AppendConfig(config, "MinidumpDir", path.value());
  std::string minidump_id = ios::device_util::GetRandomId();
  AppendConfig(config, "MinidumpID", minidump_id);
  AppendConfig(config, "BreakpadProductDisplay", breakpad_product_display);
  AppendConfig(config, "BreakpadProduct", breakpad_product);
  // UTE is not reported if app was upgrated, so the previous session had the
  // same version.
  AppendConfig(config, "BreakpadVersion", breakpad_version);
  AppendConfig(config, "BreakpadURL", breakpad_url);
  AppendConfig(config, "BreakpadMinidumpLocation", path.value());
  PreviousSessionInfo* previous_session = [PreviousSessionInfo sharedInstance];
  AppendConfig(config, "BreakpadServerParameterPrefix_free_disk_in_kb",
               base::NumberToString(previous_session.availableDeviceStorage));
  if (previous_session.didSeeMemoryWarningShortlyBeforeTerminating) {
    AppendConfig(config,
                 "BreakpadServerParameterPrefix_memory_warning_in_progress",
                 "yes");
  }
  AppendConfig(config, "BreakpadServerParameterPrefix_platform",
               base::SysInfo::HardwareModelName());

  // Write empty minidump file, as Breakpad can't upload config without the
  // minidump.
  base::File minidump_file(
      path.Append(minidump_id).AddExtension("dmp"),
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  base::FilePath config_file_path = path.Append("Config-XXXXXX");
  // const_cast is OK since mkstemp just replaces characters in place.
  base::ScopedFD config_fd(
      mkstemp(const_cast<char*>(config_file_path.value().c_str())));

  std::string config_string = base::JoinString(config, "\n");

  // Write config into memory mapped file after minidump is written, otherwise
  // Breakpad may fail to upload config without minidump.
  base::MemoryMappedFile mapped_config_file;
  const base::MemoryMappedFile::Region region = {0, config_string.size()};
  bool created_config =
      mapped_config_file.Initialize(base::File(std::move(config_fd)), region,
                                    base::MemoryMappedFile::READ_WRITE_EXTEND);
  if (created_config) {
    std::strcpy(reinterpret_cast<char*>(mapped_config_file.data()),
                config_string.data());
  }
}
