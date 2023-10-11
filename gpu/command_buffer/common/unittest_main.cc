// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_io_thread.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/gfx/linux/gbm_util.h"  // nogncheck
#endif

namespace {

class GpuTestSuite : public base::TestSuite {
 public:
  GpuTestSuite(int argc, char** argv);

  GpuTestSuite(const GpuTestSuite&) = delete;
  GpuTestSuite& operator=(const GpuTestSuite&) = delete;

  ~GpuTestSuite() override;
};

GpuTestSuite::GpuTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(b/271455200): the FeatureList has not been initialized by this point,
  // so this call will always disable Intel media compression. We may want to
  // move this to a later point to be able to run GPU unit tests with Intel
  // media compression enabled.
  ui::EnsureIntelMediaCompressionEnvVarIsSet();
#endif
}

GpuTestSuite::~GpuTestSuite() = default;

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  GpuTestSuite test_suite(argc, argv);

  mojo::core::Init();

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
