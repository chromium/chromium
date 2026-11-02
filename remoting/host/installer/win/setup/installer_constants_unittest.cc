// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/installer_constants.h"

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::installer {

TEST(InstallerConstantsTest, GetDefaultInstallDir_NotEmpty) {
  base::FilePath install_dir = GetDefaultInstallDir();
  EXPECT_FALSE(install_dir.empty());
}

TEST(InstallerConstantsTest, GetDefaultHostConfigPath_NotEmpty) {
  base::FilePath config_path = GetDefaultHostConfigPath();
  EXPECT_FALSE(config_path.empty());
  EXPECT_EQ(config_path.BaseName().value(), kHostConfigFileName);
}

}  // namespace remoting::installer
