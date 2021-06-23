// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_settings.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "remoting/host/file_host_settings.h"

#if defined(OS_APPLE)
#include "base/files/file_path.h"
#include "remoting/host/mac/constants_mac.h"
#endif  // defined(OS_APPLE)

#if defined(OS_LINUX)
#include "remoting/host/linux/file_path_util.h"
#endif  // defined(OS_LINUX)

namespace remoting {

namespace {

class EmptyHostSettings : public HostSettings {
 public:
  std::string GetString(const HostSettingKey key) const override {
    return std::string();
  }

  void SetString(const HostSettingKey key, const std::string& value) override {}

  void InitializeInstance() override {}
};

}  // namespace

// static
void HostSettings::Initialize() {
  GetInstance()->InitializeInstance();
}

HostSettings::HostSettings() = default;

HostSettings::~HostSettings() = default;

// static
HostSettings* HostSettings::GetInstance() {
#if defined(OS_APPLE)
  static const base::FilePath settings_file(kHostSettingsFilePath);
  static base::NoDestructor<FileHostSettings> instance(settings_file);
#elif defined(OS_LINUX)
  static base::NoDestructor<FileHostSettings> instance(base::FilePath(
      GetConfigDirectoryPath().Append(GetHostHash() + ".settings.json")));
#else
  // HostSettings is currently neither implemented nor used on other platforms.
  static base::NoDestructor<EmptyHostSettings> instance;
#endif
  return instance.get();
}

}  // namespace remoting
