// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_test_suite.h"

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "content/browser/webui/content_web_ui_configs.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
#include "gpu/ipc/test_gpu_thread_holder.h"
#include "media/base/media.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/dpi.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/test/mock_chrome_application_mac.h"
#endif

namespace content {

ContentTestSuite::ContentTestSuite(int argc, char** argv)
    : ContentTestSuiteBase(argc, argv) {
}

ContentTestSuite::~ContentTestSuite() = default;

void ContentTestSuite::Initialize() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool is_child_process = command_line->HasSwitch(switches::kTestChildProcess);
#if BUILDFLAG(IS_MAC)
  base::apple::ScopedNSAutoreleasePool autorelease_pool;
  // Initializing `NSApplication` before applying the sandbox profile in child
  // processes opens XPC connections that should be disallowed. We don't want or
  // need `NSApplication` in sandboxed processes anyway, so skip initializing.
  if (!is_child_process) {
    mock_cr_app::RegisterMockCrApp();
  }
#endif

#if BUILDFLAG(IS_WIN)
  display::win::SetDefaultDeviceScaleFactor(1.0f);
#endif

  InitializeResourceBundle();

  ForceInProcessNetworkService();

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
  if (!is_child_process) {
    gl::GLDisplay* display =
        gl::GLSurfaceTestSupport::InitializeNoExtensionsOneOff();
    auto* gpu_feature_info = gpu::GetTestGpuThreadHolder()->GetGpuFeatureInfo();
    gl::init::SetDisabledExtensionsPlatform(
        gpu_feature_info->disabled_extensions);
    gl::init::InitializeExtensionSettingsOneOffPlatform(display);
  }

  RegisterContentWebUIConfigs();
}

}  // namespace content
