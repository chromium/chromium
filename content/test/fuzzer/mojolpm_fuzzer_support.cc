// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fuzzer/mojolpm_fuzzer_support.h"

#include "base/command_line.h"
#include "base/debug/asan_service.h"
#include "base/i18n/icu_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "content/browser/network_service_instance_impl.h"  // [nogncheck]
#include "content/browser/storage_partition_impl.h"         // [nogncheck]
#include "content/browser/storage_partition_impl_map.h"     // [nogncheck]

namespace content {
namespace mojolpm {

#if defined(ADDRESS_SANITIZER)
static void FalsePositiveErrorReportCallback(const char* reason,
                                             bool* should_exit_cleanly) {
  if (!strcmp(base::PlatformThread::GetName(), "fuzzer_thread")) {
    base::debug::AsanService::GetInstance()->Log(
        "MojoLPM: FALSE POSITIVE\n"
        "This crash occurred on the fuzzer thread, so it is a false positive "
        "and "
        "\ndoes not represent a security issue. In MojoLPM, the fuzzer thread "
        "\nrepresents the unprivileged renderer process.\n");
    *should_exit_cleanly = true;
  }
}

static void AddFalsePositiveErrorReportCallback() {
  static bool registered = false;
  if (!registered) {
    base::debug::AsanService::GetInstance()->AddErrorCallback(
        FalsePositiveErrorReportCallback);
    registered = true;
  }
}
#endif  // defined(ADDRESS_SANITIZER)

FuzzerEnvironment::FuzzerEnvironment(int argc, const char* const* argv)
    : command_line_initialized_(base::CommandLine::Init(argc, argv)),
      fuzzer_thread_("fuzzer_thread") {
  TestTimeouts::Initialize();

  logging::SetMinLogLevel(logging::LOGGING_FATAL);
  mojo::core::Init();
  base::i18n::InitializeICU();

  ForceCreateNetworkServiceDirectlyForTesting();
  StoragePartitionImpl::ForceInProcessStorageServiceForTesting();

  fuzzer_thread_.StartAndWaitForTesting();

#if defined(ADDRESS_SANITIZER)
  base::debug::AsanService::GetInstance()->Initialize();
  AddFalsePositiveErrorReportCallback();
#endif  // defined(ADDRESS_SANITIZER)
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

BrowserTaskEnvironment* RenderViewHostTestHarnessAdapter::task_environment() {
  return RenderViewHostTestHarness::task_environment();
}

BrowserContext* RenderViewHostTestHarnessAdapter::browser_context() {
  return RenderViewHostTestHarness::browser_context();
}

}  // namespace mojolpm
}  // namespace content
