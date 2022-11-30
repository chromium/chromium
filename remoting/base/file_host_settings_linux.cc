// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/file_host_settings.h"

#include "remoting/base/file_path_util_linux.h"

namespace remoting {

base::FilePath FileHostSettings::GetSettingsFilePath() {
  return (base::FilePath(
      GetConfigDirectoryPath().Append(GetHostHash() + ".settings.json")));
}

}  // namespace remoting
