// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_host_settings.h"

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "remoting/base/logging.h"

namespace remoting {

FileHostSettings::FileHostSettings(const base::FilePath& settings_file)
    : settings_file_(settings_file) {}

FileHostSettings::~FileHostSettings() = default;

void FileHostSettings::InitializeInstance() {
  // TODO(yuweih): Make FileHostSettings detect changes of the settings file.
  if (settings_) {
    return;
  }

  if (!base::PathIsReadable(settings_file_)) {
    HOST_LOG << "Host settings file " << settings_file_ << " does not exist.";
    return;
  }
  JSONFileValueDeserializer deserializer(settings_file_);
  int error_code;
  std::string error_message;
  settings_ = deserializer.Deserialize(&error_code, &error_message);
  if (!settings_) {
    LOG(WARNING) << "Failed to load " << settings_file_
                 << ". Code: " << error_code << ", message: " << error_message;
    return;
  }
  HOST_LOG << "Host settings loaded.";
}

std::string FileHostSettings::GetString(const HostSettingKey key) const {
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

}  // namespace remoting
