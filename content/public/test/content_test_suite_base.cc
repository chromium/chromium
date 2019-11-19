// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_test_suite_base.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "content/browser/gpu/gpu_main_thread_factory.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/utility_process_host.h"
#include "content/common/url_schemes.h"
#include "content/gpu/in_process_gpu_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/in_process_renderer_thread.h"
#include "content/utility/in_process_utility_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/ui_base_paths.h"

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#include "gin/v8_initializer.h"  // nogncheck
#endif

namespace content {

namespace {
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if defined(USE_V8_CONTEXT_SNAPSHOT)
constexpr gin::V8Initializer::V8SnapshotFileType kSnapshotType =
    gin::V8Initializer::V8SnapshotFileType::kWithAdditionalContext;
#else
constexpr gin::V8Initializer::V8SnapshotFileType kSnapshotType =
    gin::V8Initializer::V8SnapshotFileType::kDefault;
#endif
#endif

// See kRunManualTestsFlag in "content_switches.cc".
const char kManualTestPrefix[] = "MANUAL_";

// Tests starting with 'MANUAL_' are skipped unless the
// command line flag "--run-manual" is supplied.
class SkipManualTests : public testing::EmptyTestEventListener {
 public:
  void OnTestStart(const testing::TestInfo& test_info) override {
    if (base::StartsWith(test_info.name(), kManualTestPrefix,
                         base::CompareCase::SENSITIVE) &&
        !base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kRunManualTestsFlag)) {
      GTEST_SKIP();
    }
  }
};

}  // namespace

ContentTestSuiteBase::ContentTestSuiteBase(int argc, char** argv)
    : base::TestSuite(argc, argv) {
}

void ContentTestSuiteBase::Initialize() {
  base::TestSuite::Initialize();
  testing::UnitTest::GetInstance()->listeners().Append(
      std::make_unique<SkipManualTests>().release());

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  gin::V8Initializer::LoadV8Snapshot(kSnapshotType);
#endif

  ui::MaterialDesignController::Initialize();
}

void ContentTestSuiteBase::RegisterContentSchemes(
    ContentClient* content_client) {
  SetContentClient(content_client);
  content::RegisterContentSchemes(false);
  SetContentClient(nullptr);
}

void ContentTestSuiteBase::RegisterInProcessThreads() {
  UtilityProcessHost::RegisterUtilityMainThreadFactory(
      CreateInProcessUtilityThread);
  RenderProcessHostImpl::RegisterRendererMainThreadFactory(
      CreateInProcessRendererThread);
  content::RegisterGpuMainThreadFactory(CreateInProcessGpuThread);
}

}  // namespace content
