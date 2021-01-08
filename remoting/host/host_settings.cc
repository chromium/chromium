// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_settings.h"

#include "base/no_destructor.h"
#include "build/build_config.h"

namespace remoting {

namespace {

class EmptyHostSettings : public HostSettings {
 public:
  std::string GetString(const HostSettingKey key) const override {
    return std::string();
  }

  void InitializeInstance() override {}
};

}  // namespace

// static
void HostSettings::Initialize() {
  GetInstance()->InitializeInstance();
}

HostSettings::HostSettings() = default;

HostSettings::~HostSettings() = default;

// HostSettings is currently neither implemented nor used on non-Mac platforms.
#if !defined(OS_APPLE)

// static
HostSettings* HostSettings::GetInstance() {
  static base::NoDestructor<EmptyHostSettings> instance;
  return instance.get();
}

#endif

}  // namespace remoting
