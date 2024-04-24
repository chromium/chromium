// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "gpu/command_buffer/tests/gl_test_setup_helper.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/scoped_nsautorelease_pool.h"
#endif

namespace {

class GlTestsSuite : public base::TestSuite {
 public:
  GlTestsSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();

    gl_setup_ = std::make_unique<gpu::GLTestSetupHelper>();
  }

 private:
  std::unique_ptr<gpu::GLTestSetupHelper> gl_setup_;
};

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  mojo::core::Init();

  // GoogleMock alters the command line, so ensure we initialize it
  // before passing it into the test suite
  testing::InitGoogleMock(&argc, argv);
  GlTestsSuite gl_tests_suite(argc, argv);
#if BUILDFLAG(IS_MAC)
  base::apple::ScopedNSAutoreleasePool pool;
#endif
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&GlTestsSuite::Run, base::Unretained(&gl_tests_suite)));
}
