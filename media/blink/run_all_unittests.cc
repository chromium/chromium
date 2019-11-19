// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "media/base/media.h"
#include "media/blink/blink_platform_with_task_environment.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/blink.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

#if !defined(OS_IOS)
#include "mojo/core/embedder/embedder.h"
#endif

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#include "gin/v8_initializer.h"

using media::BlinkPlatformWithTaskEnvironment;

constexpr gin::V8Initializer::V8SnapshotFileType kSnapshotType =
#if defined(USE_V8_CONTEXT_SNAPSHOT)
    gin::V8Initializer::V8SnapshotFileType::kWithAdditionalContext;
#else
    gin::V8Initializer::V8SnapshotFileType::kDefault;
#endif  // defined(USE_V8_CONTEXT_SNAPSHOT)
#endif  // defined(V8_USE_EXTERNAL_STARTUP_DATA)

class MediaBlinkTestSuite : public base::TestSuite {
 public:
  MediaBlinkTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 private:
  void Initialize() override {
    base::TestSuite::Initialize();

#if defined(OS_ANDROID)
    if (media::MediaCodecUtil::IsMediaCodecAvailable())
      media::EnablePlatformDecoderSupport();
#endif

    // Run this here instead of main() to ensure an AtExitManager is already
    // present.
    media::InitializeMediaLibrary();

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
    gin::V8Initializer::LoadV8Snapshot(kSnapshotType);
#endif

#if !defined(OS_IOS)
    // Initialize mojo firstly to enable Blink initialization to use it.
    mojo::core::Init();
#endif

    platform_ = std::make_unique<BlinkPlatformWithTaskEnvironment>();

    mojo::BinderMap binders;
    blink::Initialize(platform_.get(), &binders,
                      platform_->GetMainThreadScheduler());
  }

  void Shutdown() override {
    platform_.reset();
    base::TestSuite::Shutdown();
  }

  std::unique_ptr<BlinkPlatformWithTaskEnvironment> platform_;

  DISALLOW_COPY_AND_ASSIGN(MediaBlinkTestSuite);
};

int main(int argc, char** argv) {
  MediaBlinkTestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindRepeating(&MediaBlinkTestSuite::Run,
                          base::Unretained(&test_suite)));
}
