// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_test_suite.h"
#include "base/memory/raw_ptr.h"

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/test_content_client_initializer.h"
#include "gpu/ipc/test_gpu_thread_holder.h"
#include "media/base/media.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if defined(OS_WIN)
#include "ui/display/win/dpi.h"
#endif

#if defined(OS_MAC)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/test/mock_chrome_application_mac.h"
#endif

namespace content {
namespace {

class TestInitializationListener : public testing::EmptyTestEventListener {
 public:
  TestInitializationListener() : test_content_client_initializer_(nullptr) {}

  TestInitializationListener(const TestInitializationListener&) = delete;
  TestInitializationListener& operator=(const TestInitializationListener&) =
      delete;

  void OnTestStart(const testing::TestInfo& test_info) override {
    test_content_client_initializer_ =
        new content::TestContentClientInitializer();
  }

  void OnTestEnd(const testing::TestInfo& test_info) override {
    delete test_content_client_initializer_;
  }

 private:
  raw_ptr<content::TestContentClientInitializer>
      test_content_client_initializer_;
};

}  // namespace

ContentTestSuite::ContentTestSuite(int argc, char** argv)
    : ContentTestSuiteBase(argc, argv) {
}

ContentTestSuite::~ContentTestSuite() = default;

void ContentTestSuite::Initialize() {
#if defined(OS_MAC)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
  mock_cr_app::RegisterMockCrApp();
#endif

#if defined(OS_WIN)
  display::win::SetDefaultDeviceScaleFactor(1.0f);
#endif

  ForceInProcessNetworkService(true);

  ContentTestSuiteBase::Initialize();
  {
    ContentClient client;
    ContentTestSuiteBase::RegisterContentSchemes(&client);
  }
  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);

  RegisterPathProvider();
  media::InitializeMediaLibrary();
  // When running in a child process for Mac sandbox tests, the sandbox exists
  // to initialize GL, so don't do it here.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool is_child_process = command_line->HasSwitch(switches::kTestChildProcess);
  if (!is_child_process) {
    gl::GLSurfaceTestSupport::InitializeNoExtensionsOneOff();
    auto* gpu_feature_info = gpu::GetTestGpuThreadHolder()->GetGpuFeatureInfo();
    gl::init::SetDisabledExtensionsPlatform(
        gpu_feature_info->disabled_extensions);
    gl::init::InitializeExtensionSettingsOneOffPlatform();
  }
  // TestEventListeners repeater event propagation is disabled in death test
  // child process.
  if (command_line->HasSwitch("gtest_internal_run_death_test")) {
    test_content_client_initializer_ =
        std::make_unique<TestContentClientInitializer>();
  } else {
    testing::TestEventListeners& listeners =
        testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new TestInitializationListener);
  }
}

void ContentTestSuite::Shutdown() {
  test_content_client_initializer_.reset();
  ContentTestSuiteBase::Shutdown();
}

}  // namespace content
