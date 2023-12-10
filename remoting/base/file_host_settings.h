// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_FILE_HOST_SETTINGS_H_
#define REMOTING_BASE_FILE_HOST_SETTINGS_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "remoting/base/host_settings.h"

namespace remoting {

// An implementation of HostSettings that reads settings from a JSON file.
// Note that this class currently doesn't watch for changes in the file.
class FileHostSettings final : public HostSettings {
 public:
  static base::FilePath GetSettingsFilePath();

  explicit FileHostSettings(const base::FilePath& settings_file);
  FileHostSettings(const FileHostSettings&) = delete;
  FileHostSettings& operator=(const FileHostSettings&) = delete;
  ~FileHostSettings() override;

  // HostSettings implementation.
  void InitializeInstance() override;
  std::string GetString(const HostSettingKey key,
                        const std::string& default_value) const override;
  void SetString(const HostSettingKey key, const std::string& value) override;

 private:
  base::FilePath settings_file_;

#if !defined(NDEBUG)
  // Used to make sure the instance is only used on the same sequenced once
  // SetString() is called.
  scoped_refptr<base::SequencedTaskRunner> task_runner_for_checking_sequence_;
#endif

  // TODO(yuweih): This needs to be guarded with a lock if we detect changes of
  // the settings file.
  std::optional<base::Value::Dict> settings_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_FILE_HOST_SETTINGS_H_
