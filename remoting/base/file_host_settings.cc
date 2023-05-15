// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/file_host_settings.h"

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_writer.h"
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
  std::unique_ptr<base::Value> settings =
      deserializer.Deserialize(&error_code, &error_message);
  if (!settings) {
    LOG(WARNING) << "Failed to load " << settings_file_
                 << ". Code: " << error_code << ", message: " << error_message;
    return;
  }
  if (!settings->is_dict()) {
    LOG(WARNING) << "Settings loaded from " << settings_file_
                 << " are not a valid json dictionary.";
    return;
  }
  settings_ = std::move(*settings).TakeDict();
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
  const std::string* string_value = settings_->FindString(key);
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
    settings_ = base::Value::Dict();
  }
  settings_->Set(key, value);

  auto json = base::WriteJson(*settings_);
  if (!json) {
    LOG(ERROR) << "Failed to serialize host settings JSON";
    return;
  }
  if (!base::ImportantFileWriter::WriteFileAtomically(settings_file_, *json)) {
    LOG(ERROR) << "Can't write host settings file to " << settings_file_;
  }
}

}  // namespace remoting
