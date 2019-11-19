// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/scale_factor.h"
#include "ui/base/ui_base_paths.h"

namespace {

class DeviceTestSuite : public base::TestSuite {
 public:
  DeviceTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}
  ~DeviceTestSuite() override = default;

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();

#if !defined(OS_IOS)
    ui::RegisterPathProvider();

    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

    base::FilePath path;
#if defined(OS_ANDROID)
    ASSERT_TRUE(base::PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &path));
#else
    ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &path));
#endif  // defined(OS_ANDROID)
    base::FilePath bluetooth_test_strings =
        path.Append(FILE_PATH_LITERAL("bluetooth_test_strings.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        bluetooth_test_strings, ui::SCALE_FACTOR_NONE);
#endif  // !defined(OS_IOS)
  }

  void Shutdown() override {
#if !defined(OS_IOS)
    ui::ResourceBundle::CleanupSharedInstance();
#endif

    base::TestSuite::Shutdown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceTestSuite);
};

}  // namespace

int main(int argc, char** argv) {
  DeviceTestSuite test_suite(argc, argv);

  mojo::core::Init();
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&DeviceTestSuite::Run, base::Unretained(&test_suite)));
}
