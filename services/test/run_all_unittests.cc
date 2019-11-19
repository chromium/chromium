// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file.h"
#include "base/i18n/icu_util.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/scale_factor.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif

namespace {

class ServiceTestSuite : public base::TestSuite {
 public:
  ServiceTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}
  ~ServiceTestSuite() override = default;

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
    ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &path));
#endif
    base::FilePath bluetooth_test_strings =
        path.Append(FILE_PATH_LITERAL("bluetooth_test_strings.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        bluetooth_test_strings, ui::SCALE_FACTOR_NONE);
#endif  // !defined(OS_IOS)

    // base::TestSuite and ViewsInit both try to load icu. That's ok for tests.
    base::i18n::AllowMultipleInitializeCallsForTesting();
  }

  void Shutdown() override {
#if !defined(OS_IOS)
    ui::ResourceBundle::CleanupSharedInstance();
#endif

    base::TestSuite::Shutdown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceTestSuite);
};

}  // namespace

int main(int argc, char** argv) {
  ServiceTestSuite test_suite(argc, argv);

  mojo::core::Configuration mojo_config;
  mojo_config.is_broker_process = true;
  mojo::core::Init(mojo_config);

  base::Thread ipc_thread("IPC thread");
  ipc_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));
  mojo::core::ScopedIPCSupport ipc_support(
      ipc_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&ServiceTestSuite::Run, base::Unretained(&test_suite)));
}
