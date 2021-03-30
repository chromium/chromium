// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_settings_mac.h"

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "remoting/base/logging.h"
#include "remoting/host/mac/constants_mac.h"

namespace remoting {

HostSettingsMac::HostSettingsMac() = default;

HostSettingsMac::~HostSettingsMac() = default;

void HostSettingsMac::InitializeInstance() {
  // TODO(yuweih): Make HostSettingsMac detect changes of the settings file.
  if (settings_) {
    return;
  }

  base::FilePath settings_file(kHostSettingsFilePath);
  if (!base::PathIsReadable(settings_file)) {
    HOST_LOG << "Host settings file " << kHostSettingsFilePath
             << " does not exist.";
    return;
  }
  JSONFileValueDeserializer deserializer(settings_file);
  int error_code;
  std::string error_message;
  settings_ = deserializer.Deserialize(&error_code, &error_message);
  if (!settings_) {
    LOG(WARNING) << "Failed to load " << kHostSettingsFilePath
                 << ". Code: " << error_code << ", message: " << error_message;
    return;
  }
  HOST_LOG << "Host settings loaded.";
}

std::string HostSettingsMac::GetString(const HostSettingKey key) const {
  if (!settings_) {
    VLOG(1) << "Either Initialize() has not been called, or the settings file "
               "doesn't exist.";
    return std::string();
  }
  std::string* string_value = settings_->FindStringKey(key);
  if (!string_value) {
    return std::string();
  }
  return *string_value;
}

HostSettings* HostSettings::GetInstance() {
  static base::NoDestructor<HostSettingsMac> instance;
  return instance.get();
}

}  // namespace remoting
