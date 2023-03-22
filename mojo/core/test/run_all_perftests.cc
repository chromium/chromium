// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/test/perf_test_suite.h"
#include "mojo/core/test/scoped_mojo_support.h"
#include "mojo/core/test/test_support_impl.h"
#include "mojo/public/tests/test_support_private.h"

int main(int argc, char** argv) {
  CHECK(base::CommandLine::Init(argc, argv));
  const auto& cmd = *base::CommandLine::ForCurrentProcess();
  auto features = std::make_unique<base::FeatureList>();
  features->InitializeFromCommandLine(
      cmd.GetSwitchValueASCII(switches::kEnableFeatures),
      cmd.GetSwitchValueASCII(switches::kDisableFeatures));
  base::FeatureList::SetInstance(std::move(features));

  base::PerfTestSuite test(argc, argv);
  mojo::core::test::ScopedMojoSupport mojo_support;
  mojo::test::TestSupport::Init(new mojo::core::test::TestSupportImpl());
  return test.Run();
}
