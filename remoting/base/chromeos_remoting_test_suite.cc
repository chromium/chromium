// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/chromeos_remoting_test_suite.h"

#include "base/path_service.h"
#include "base/test/test_suite.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/gl/test/gl_surface_test_support.h"
#endif

namespace remoting {

ChromeOSRemotingTestSuite::ChromeOSRemotingTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

ChromeOSRemotingTestSuite::~ChromeOSRemotingTestSuite() = default;

void ChromeOSRemotingTestSuite::Initialize() {
  base::TestSuite::Initialize();
  gl::GLSurfaceTestSupport::InitializeOneOff();
  ui::RegisterPathProvider();

  base::FilePath ui_test_pak_path;
  ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
  ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("en-US");
}

void ChromeOSRemotingTestSuite::Shutdown() {
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

}  // namespace remoting
