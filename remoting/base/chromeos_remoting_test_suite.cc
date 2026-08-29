// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/chromeos_remoting_test_suite.h"

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace remoting {

ChromeOSRemotingTestSuite::ChromeOSRemotingTestSuite(int argc, char** argv)
    : ash::AshTestSuite(argc, argv) {}

ChromeOSRemotingTestSuite::~ChromeOSRemotingTestSuite() = default;

void ChromeOSRemotingTestSuite::Initialize() {
  ash::AshTestSuite::Initialize();

  base::FilePath pak_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &pak_path));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_path.AppendASCII("remoting_locales").AppendASCII("en-US.pak"),
      ui::kScaleFactorNone);
}

void ChromeOSRemotingTestSuite::Shutdown() {
  ash::AshTestSuite::Shutdown();
}

}  // namespace remoting
