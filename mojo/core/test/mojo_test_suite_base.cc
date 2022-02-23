// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/test/mojo_test_suite_base.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {
namespace test {

namespace {

// Manual tests only run when --run-manual is specified. This allows writing
// tests that don't run automatically but are still in the same test binary.
// This is useful so that a team that wants to run a few tests doesn't have to
// add a new binary that must be compiled on all builds.
constexpr char kRunManualTestsFlag[] = "run-manual";

// Tests starting with 'MANUAL_' are skipped unless the
// command line flag `kRunManualTestsFlag`  is supplied.
constexpr char kManualTestPrefix[] = "MANUAL_";

class SkipManualTests : public testing::EmptyTestEventListener {
 public:
  void OnTestStart(const testing::TestInfo& test_info) override {
    if (base::StartsWith(test_info.name(), kManualTestPrefix,
                         base::CompareCase::SENSITIVE) &&
        !base::CommandLine::ForCurrentProcess()->HasSwitch(
            kRunManualTestsFlag)) {
      GTEST_SKIP();
    }
  }
};

}  // namespace

MojoTestSuiteBase::MojoTestSuiteBase(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

#if BUILDFLAG(IS_WIN)
MojoTestSuiteBase::MojoTestSuiteBase(int argc, wchar_t** argv)
    : base::TestSuite(argc, argv) {}
#endif  // BUILDFLAG(IS_WIN)

void MojoTestSuiteBase::Initialize() {
  base::TestSuite::Initialize();
  testing::UnitTest::GetInstance()->listeners().Append(
      std::make_unique<SkipManualTests>().release());
}

}  // namespace test
}  // namespace core
}  // namespace mojo
