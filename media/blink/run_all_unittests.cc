// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "media/base/media.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/web/blink.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

#if !defined(OS_IOS)
#include "mojo/core/embedder/embedder.h"
#endif

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#include "gin/v8_initializer.h"

constexpr gin::V8Initializer::V8SnapshotFileType kSnapshotType =
#if defined(USE_V8_CONTEXT_SNAPSHOT)
    gin::V8Initializer::V8SnapshotFileType::kWithAdditionalContext;
#else
    gin::V8Initializer::V8SnapshotFileType::kDefault;
#endif  // defined(USE_V8_CONTEXT_SNAPSHOT)
#endif  // defined(V8_USE_EXTERNAL_STARTUP_DATA)

// We must use a custom blink::Platform that ensures the main thread scheduler
// knows about the ScopedTaskEnvironment.
class BlinkPlatformWithTaskEnvironment : public blink::Platform {
 public:
  BlinkPlatformWithTaskEnvironment()
      : main_thread_scheduler_(
            blink::scheduler::WebThreadScheduler::CreateMainThreadScheduler()) {
  }

  ~BlinkPlatformWithTaskEnvironment() override {
    main_thread_scheduler_->Shutdown();
  }

  blink::scheduler::WebThreadScheduler* GetMainThreadScheduler() {
    return main_thread_scheduler_.get();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<blink::scheduler::WebThreadScheduler> main_thread_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(BlinkPlatformWithTaskEnvironment);
};

static int RunTests(base::TestSuite* test_suite) {
#if defined(OS_ANDROID)
  if (media::MediaCodecUtil::IsMediaCodecAvailable())
    media::EnablePlatformDecoderSupport();
#endif

  // Run this here instead of main() to ensure an AtExitManager is already
  // present.
  media::InitializeMediaLibrary();

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  gin::V8Initializer::LoadV8Snapshot(kSnapshotType);
  gin::V8Initializer::LoadV8Natives();
#endif

#if !defined(OS_IOS)
  // Initialize mojo firstly to enable Blink initialization to use it.
  mojo::core::Init();
#endif

  BlinkPlatformWithTaskEnvironment platform_;
  service_manager::BinderRegistry empty_registry;
  blink::Initialize(&platform_, &empty_registry,
                    platform_.GetMainThreadScheduler());

  return test_suite->Run();
}

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindRepeating(&RunTests, base::Unretained(&test_suite)));
}
