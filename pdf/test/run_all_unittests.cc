// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/task_environment.h"
#include "base/test/test_suite.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/web/blink.h"
#include "v8/include/v8.h"

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#include "gin/v8_initializer.h"
#endif

namespace {

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
constexpr gin::V8Initializer::V8SnapshotFileType kSnapshotType =
#if defined(USE_V8_CONTEXT_SNAPSHOT)
    gin::V8Initializer::V8SnapshotFileType::kWithAdditionalContext;
#else
    gin::V8Initializer::V8SnapshotFileType::kDefault;
#endif  // defined(USE_V8_CONTEXT_SNAPSHOT)
#endif  // defined(V8_USE_EXTERNAL_STARTUP_DATA)

class BlinkPlatformForTesting : public blink::Platform {
 public:
  BlinkPlatformForTesting() = default;
  BlinkPlatformForTesting(const BlinkPlatformForTesting&) = delete;
  BlinkPlatformForTesting& operator=(const BlinkPlatformForTesting&) = delete;
  ~BlinkPlatformForTesting() override { main_thread_scheduler_->Shutdown(); }

  blink::scheduler::WebThreadScheduler* GetMainThreadScheduler() {
    return main_thread_scheduler_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<blink::scheduler::WebThreadScheduler> main_thread_scheduler_ =
      blink::scheduler::CreateWebMainThreadSchedulerForTests();
};

class PdfTestSuite final : public base::TestSuite {
 public:
  using TestSuite::TestSuite;
  PdfTestSuite(const PdfTestSuite&) = delete;
  PdfTestSuite& operator=(const PdfTestSuite&) = delete;

 protected:
  void Initialize() override {
    TestSuite::Initialize();

    mojo::core::Init();

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
    gin::V8Initializer::LoadV8Snapshot(kSnapshotType);
#endif

    blink::Platform::InitializeBlink();
    platform_ = std::make_unique<BlinkPlatformForTesting>();

    mojo::BinderMap binders;
    blink::Initialize(platform_.get(), &binders,
                      platform_->GetMainThreadScheduler());
  }

  void Shutdown() override {
    platform_.reset();
    base::TestSuite::Shutdown();
  }

 private:
  std::unique_ptr<BlinkPlatformForTesting> platform_;
};

}  // namespace

int main(int argc, char** argv) {
  PdfTestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&PdfTestSuite::Run, base::Unretained(&test_suite)));
}
