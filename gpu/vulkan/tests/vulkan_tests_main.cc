// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/task_environment.h"
#include "base/test/test_suite.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/platform/platform_event_source.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace {

class VulkanTestSuite : public base::TestSuite {
 public:
  VulkanTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();

    task_environment_ = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::MainThreadType::UI);
    platform_event_source_ = ui::PlatformEventSource::CreateDefault();

#if defined(USE_OZONE)
    // Make Ozone run in single-process mode.
    ui::OzonePlatform::InitParams params;
    params.single_process = true;

    // This initialization must be done after TaskEnvironment has
    // initialized the UI thread.
    ui::OzonePlatform::InitializeForUI(params);
    ui::OzonePlatform::InitializeForGPU(params);
#endif
  }

 private:
  std::unique_ptr<ui::PlatformEventSource> platform_event_source_;
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};

}  // namespace

int main(int argc, char** argv) {
  VulkanTestSuite test_suite(argc, argv);
  base::CommandLine::Init(argc, argv);
  testing::InitGoogleMock(&argc, argv);
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&VulkanTestSuite::Run, base::Unretained(&test_suite)));
}
