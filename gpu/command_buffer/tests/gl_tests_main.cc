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

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/gfx/linux/gbm_util.h"  // nogncheck
#endif

namespace {

class GlTestsSuite : public base::TestSuite {
 public:
  GlTestsSuite(int argc, char** argv) : base::TestSuite(argc, argv) {
#if BUILDFLAG(IS_CHROMEOS)
    // TODO(b/271455200): the FeatureList has not been initialized by this
    // point, so this call will always disable Intel media compression. We may
    // want to move this to a later point to be able to run GL unit tests with
    // Intel media compression enabled.
    ui::EnsureIntelMediaCompressionEnvVarIsSet();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

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

  GlTestsSuite gl_tests_suite(argc, argv);
#if BUILDFLAG(IS_MAC)
  base::apple::ScopedNSAutoreleasePool pool;
#endif
  testing::InitGoogleMock(&argc, argv);
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&GlTestsSuite::Run, base::Unretained(&gl_tests_suite)));
}
