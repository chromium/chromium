// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fuzzer/mojolpm_fuzzer_support.h"

#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/test/test_timeouts.h"
#include "content/browser/network_service_instance_impl.h"  // [nogncheck]
#include "content/browser/storage_partition_impl.h"         // [nogncheck]
#include "content/browser/storage_partition_impl_map.h"     // [nogncheck]

namespace content {
namespace mojolpm {
FuzzerEnvironment::FuzzerEnvironment(int argc, const char* const* argv)
    : command_line_initialized_(base::CommandLine::Init(argc, argv)),
      fuzzer_thread_("fuzzer_thread") {
  TestTimeouts::Initialize();

  logging::SetMinLogLevel(logging::LOG_FATAL);
  mojo::core::Init();
  base::i18n::InitializeICU();

  ForceCreateNetworkServiceDirectlyForTesting();
  StoragePartitionImpl::ForceInProcessStorageServiceForTesting();

  fuzzer_thread_.StartAndWaitForTesting();
}

FuzzerEnvironment::~FuzzerEnvironment() {}

FuzzerEnvironmentWithTaskEnvironment::FuzzerEnvironmentWithTaskEnvironment(
    int argc,
    const char* const* argv)
    : FuzzerEnvironment(argc, argv),
      task_environment_(
          base::test::TaskEnvironment::MainThreadType::DEFAULT,
          base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC,
          base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
          BrowserTaskEnvironment::REAL_IO_THREAD) {}

FuzzerEnvironmentWithTaskEnvironment::~FuzzerEnvironmentWithTaskEnvironment() {}

RenderViewHostTestHarnessAdapter::RenderViewHostTestHarnessAdapter()
    : RenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME,
          base::test::TaskEnvironment::MainThreadType::DEFAULT,
          base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC,
          base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
          BrowserTaskEnvironment::REAL_IO_THREAD) {}

RenderViewHostTestHarnessAdapter::~RenderViewHostTestHarnessAdapter() {}

void RenderViewHostTestHarnessAdapter::SetUp() {
  RenderViewHostTestHarness::SetUp();
}

void RenderViewHostTestHarnessAdapter::TearDown() {
  RenderViewHostTestHarness::TearDown();
}
}  // namespace mojolpm
}  // namespace content