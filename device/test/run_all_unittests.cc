// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/base/ui_base_paths.h"

namespace {

class DeviceTestSuite : public base::TestSuite {
 public:
  DeviceTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

  DeviceTestSuite(const DeviceTestSuite&) = delete;
  DeviceTestSuite& operator=(const DeviceTestSuite&) = delete;

  ~DeviceTestSuite() override = default;

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();

#if !BUILDFLAG(IS_IOS)
    ui::RegisterPathProvider();

    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

    base::FilePath path;
#if BUILDFLAG(IS_ANDROID)
    ASSERT_TRUE(base::PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &path));
#else
    ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &path));
#endif  // BUILDFLAG(IS_ANDROID)
    base::FilePath bluetooth_test_strings =
        path.Append(FILE_PATH_LITERAL("bluetooth_test_strings.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        bluetooth_test_strings, ui::kScaleFactorNone);
#endif  // !BUILDFLAG(IS_IOS)
  }

  void Shutdown() override {
#if !BUILDFLAG(IS_IOS)
    ui::ResourceBundle::CleanupSharedInstance();
#endif

    base::TestSuite::Shutdown();
  }
};

}  // namespace

int main(int argc, char** argv) {
  DeviceTestSuite test_suite(argc, argv);

  mojo::core::Init();
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&DeviceTestSuite::Run, base::Unretained(&test_suite)));
}
