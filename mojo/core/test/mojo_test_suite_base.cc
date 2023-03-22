// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/test/mojo_test_suite_base.h"

#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "mojo/core/test/scoped_mojo_support.h"
#include "mojo/core/test/test_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::core::test {

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

// Ensures that every test gets its own freshly initialized Mojo Core instance
// and IO dedicated thread.
class MojoSupportForEachTest : public testing::EmptyTestEventListener {
 public:
  MojoSupportForEachTest() = default;
  ~MojoSupportForEachTest() override = default;

  MojoSupportForEachTest(const MojoSupportForEachTest&) = delete;
  MojoSupportForEachTest& operator=(const MojoSupportForEachTest&) = delete;

  void OnTestStart(const testing::TestInfo& test_info) override {
    // base::TestSuite hooks should have already initialized this.
    CHECK(base::FeatureList::GetInstance());
    mojo_support_ = std::make_unique<ScopedMojoSupport>();
  }

  void OnTestEnd(const testing::TestInfo& test_info) override {
    mojo_support_.reset();
  }

 private:
  std::unique_ptr<ScopedMojoSupport> mojo_support_;
};

}  // namespace

MojoTestSuiteBase::MojoTestSuiteBase(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

#if BUILDFLAG(IS_WIN)
MojoTestSuiteBase::MojoTestSuiteBase(int argc, wchar_t** argv)
    : base::TestSuite(argc, argv) {}
#endif  // BUILDFLAG(IS_WIN)

MojoTestSuiteBase::~MojoTestSuiteBase() = default;

void MojoTestSuiteBase::Initialize() {
  base::TestSuite::Initialize();

  auto& listeners = testing::UnitTest::GetInstance()->listeners();
  listeners.Append(std::make_unique<SkipManualTests>().release());
  listeners.Append(std::make_unique<MojoSupportForEachTest>().release());

  MaybeInitializeChildProcessEnvironment();
}

void MojoTestSuiteBase::Shutdown() {
  child_mojo_support_.reset();
  child_feature_list_.reset();
}

void MojoTestSuiteBase::MaybeInitializeChildProcessEnvironment() {
  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  const char kDeathTestSwitch[] = "gtest_internal_run_death_test";
  if (!command_line.HasSwitch(switches::kTestChildProcess) &&
      !command_line.HasSwitch(kDeathTestSwitch)) {
    // Not in a child process. Do nothing.
    return;
  }

  const std::string enabled =
      command_line.GetSwitchValueASCII(switches::kEnableFeatures);
  const std::string disabled =
      command_line.GetSwitchValueASCII(switches::kDisableFeatures);

  child_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  child_feature_list_->InitFromCommandLine(enabled, disabled);

  if (!command_line.HasSwitch(test_switches::kNoMojo)) {
    child_mojo_support_ = std::make_unique<ScopedMojoSupport>();
  }
}

}  // namespace mojo::core::test
