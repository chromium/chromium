// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/unittest_test_suite.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "content/browser/network_service_instance_impl.h"
#include "content/public/test/test_host_resolver.h"
#include "content/test/test_blink_web_unit_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/blink.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

#if defined(USE_X11)
#include "ui/gfx/x/x11.h"
#endif

#if defined(OS_FUCHSIA)
#include "ui/ozone/public/ozone_switches.h"
#endif

namespace content {

namespace {

// The global NetworkService object could be created in some tests due to
// various StoragePartition calls. Since it has a mojo pipe that is bound using
// the current thread, which goes away between tests, we need to destruct it to
// avoid calls being dropped silently.
class ResetNetworkServiceBetweenTests : public testing::EmptyTestEventListener {
 public:
  ResetNetworkServiceBetweenTests() = default;

  void OnTestEnd(const testing::TestInfo& test_info) override {
    // If the network::NetworkService object was instantiated during a unit test
    // it will be deleted because network_service_instance.cc has it in a
    // SequenceLocalStorageSlot. However we want to synchronously destruct the
    // InterfacePtr pointing to it to avoid it getting the connection error
    // later and have other tests use the InterfacePtr that is invalid.
    ResetNetworkServiceForTesting();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ResetNetworkServiceBetweenTests);
};

}  // namespace

UnitTestTestSuite::UnitTestTestSuite(base::TestSuite* test_suite)
    : test_suite_(test_suite) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string enabled =
      command_line->GetSwitchValueASCII(switches::kEnableFeatures);
  std::string disabled =
      command_line->GetSwitchValueASCII(switches::kDisableFeatures);

  ForceCreateNetworkServiceDirectlyForTesting();

  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new ResetNetworkServiceBetweenTests);

  // The ThreadPool created by the test launcher is never destroyed.
  // Similarly, the FeatureList created here is never destroyed so it
  // can safely be accessed by the ThreadPool.
  std::unique_ptr<base::FeatureList> feature_list =
      std::make_unique<base::FeatureList>();
  feature_list->InitializeFromCommandLine(enabled, disabled);
  base::FeatureList::SetInstance(std::move(feature_list));

#if defined(OS_FUCHSIA)
  // Use headless ozone platform on Fuchsia by default.
  // TODO(crbug.com/865172): Remove this flag.
  if (!command_line->HasSwitch(switches::kOzonePlatform))
    command_line->AppendSwitchASCII(switches::kOzonePlatform, "headless");
#endif

#if defined(USE_X11)
  XInitThreads();
#endif
  DCHECK(test_suite);
  blink_test_support_.reset(new TestBlinkWebUnitTestSupport);
  test_host_resolver_ = std::make_unique<TestHostResolver>();
}

UnitTestTestSuite::~UnitTestTestSuite() = default;

int UnitTestTestSuite::Run() {
#if defined(USE_AURA)
  std::unique_ptr<aura::Env> aura_env = aura::Env::CreateInstance();
#endif

  return test_suite_->Run();
}

}  // namespace content
