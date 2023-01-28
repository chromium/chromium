// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/file_host_settings.h"

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
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

std::string FileHostSettings::GetString(
    const HostSettingKey key,
    const std::string& default_value) const {
#if !defined(NDEBUG)
  if (task_runner_for_checking_sequence_) {
    DCHECK(task_runner_for_checking_sequence_->RunsTasksInCurrentSequence());
  }
#endif

  if (!settings_) {
    VLOG(1) << "Either Initialize() has not been called, or the settings file "
               "doesn't exist.";
    return default_value;
  }
  std::string* string_value = settings_->FindStringKey(key);
  if (!string_value) {
    return default_value;
  }
  return *string_value;
}

void FileHostSettings::SetString(const HostSettingKey key,
                                 const std::string& value) {
#if !defined(NDEBUG)
  if (task_runner_for_checking_sequence_) {
    DCHECK(task_runner_for_checking_sequence_->RunsTasksInCurrentSequence());
  } else {
    task_runner_for_checking_sequence_ =
        base::SequencedTaskRunner::GetCurrentDefault();
  }
#endif

  if (!settings_) {
    VLOG(1) << "Settings file didn't exist. New file will be created.";
    settings_ = std::make_unique<base::Value>(base::Value::Type::DICT);
  }
  settings_->SetStringKey(key, value);

  std::string json;
  JSONStringValueSerializer serializer(&json);
  if (!serializer.Serialize(*settings_)) {
    LOG(ERROR) << "Failed to serialize host settings JSON";
    return;
  }
  if (!base::ImportantFileWriter::WriteFileAtomically(settings_file_, json)) {
    LOG(ERROR) << "Can't write host settings file to " << settings_file_;
  }
}

}  // namespace remoting
