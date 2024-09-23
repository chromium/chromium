// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/blink_test_environment.h"

#include <string>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_tokenizer.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_suite_helper.h"
#include "build/build_config.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/test/test_blink_web_unit_test_support.h"
#include "mojo/core/embedder/embedder.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/blink.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/dpi.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/test/mock_chrome_application_mac.h"
#endif

namespace content {

BlinkTestEnvironment::BlinkTestEnvironment() = default;
BlinkTestEnvironment::~BlinkTestEnvironment() = default;

void BlinkTestEnvironment::SetUp() {
  base::test::InitScopedFeatureListForTesting(scoped_feature_list_);
  blink::WebRuntimeFeatures::EnableExperimentalFeatures(true);
  blink::WebRuntimeFeatures::EnableTestOnlyFeatures(true);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  for (const std::string& feature : content::FeaturesFromSwitch(
           command_line, switches::kEnableBlinkFeatures)) {
    blink::WebRuntimeFeatures::EnableFeatureFromString(feature, true);
  }
  for (const std::string& feature : content::FeaturesFromSwitch(
           command_line, switches::kDisableBlinkFeatures)) {
    blink::WebRuntimeFeatures::EnableFeatureFromString(feature, false);
  }

#if BUILDFLAG(IS_MAC)
  mock_cr_app::RegisterMockCrApp();
#endif

#if BUILDFLAG(IS_WIN)
  display::win::SetDefaultDeviceScaleFactor(1.0f);
#endif

  content_initializer_.emplace();

  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);
  content::ContentTestSuiteBase::InitializeResourceBundle();

  // TestBlinkWebUnitTestSupport construction needs Mojo to be initialized
  // first.
  mojo::core::Init(mojo::core::Configuration{.is_broker_process = true});

  InitializeBlinkTestSupport();
}

void BlinkTestEnvironment::InitializeBlinkTestSupport() {
  // Depends on resource bundle initialization so has to happen after.
  blink_test_support_ = std::make_unique<content::TestBlinkWebUnitTestSupport>(
      content::TestBlinkWebUnitTestSupport::SchedulerType::kMockScheduler);
}

void BlinkTestEnvironment::TearDown() {
  blink_test_support_.reset();
  content_initializer_.reset();
  scoped_feature_list_.Reset();
}

void BlinkTestEnvironmentWithIsolate::TearDown() {
  // Flush any remaining messages before we kill ourselves. Unlike
  // BlinkTestEnvironment, this is needed here because kRealScheduler is used
  // for TestBlinkWebUnitTestSupport, which instantiates a
  // MainThreadSchedulerImpl and doesn't automatically flushes tasks.
  // http://code.google.com/p/chromium/issues/detail?id=9500
  base::RunLoop().RunUntilIdle();

  BlinkTestEnvironment::TearDown();
}

void BlinkTestEnvironmentWithIsolate::InitializeBlinkTestSupport() {
  // Depends on resource bundle initialization so has to happen after.
  blink_test_support_ = std::make_unique<content::TestBlinkWebUnitTestSupport>(
      content::TestBlinkWebUnitTestSupport::SchedulerType::kRealScheduler);
  isolate_ = blink::CreateMainThreadIsolate();
}

}  // namespace content
