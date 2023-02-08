// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/host_settings.h"

#include "base/no_destructor.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE) || (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS))
#include "remoting/base/file_host_settings.h"
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
#include "remoting/base/host_settings_win.h"
#endif  // BUILDFLAG(IS_WIN)

namespace remoting {

namespace {

class EmptyHostSettings : public HostSettings {
 public:
  std::string GetString(const HostSettingKey key,
                        const std::string& default_value) const override {
    return default_value;
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
#if BUILDFLAG(IS_APPLE) || (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS))
  static base::NoDestructor<FileHostSettings> instance(
      FileHostSettings::GetSettingsFilePath());
#elif BUILDFLAG(IS_WIN)
  static base::NoDestructor<HostSettingsWin> instance;
#else
  // HostSettings is currently neither implemented nor used on other platforms.
  static base::NoDestructor<EmptyHostSettings> instance;
#endif
  return instance.get();
}

}  // namespace remoting
