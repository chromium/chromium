// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "media/base/fake_localized_strings.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "mojo/core/embedder/embedder.h"

#if BUILDFLAG(IS_ANDROID)
#include "media/base/android/media_codec_bridge_impl.h"
#include "media/base/android/media_codec_util.h"
#endif

class TestSuiteNoAtExit : public base::TestSuite {
 public:
  TestSuiteNoAtExit(int argc, char** argv) : TestSuite(argc, argv) {}
  ~TestSuiteNoAtExit() override = default;

 protected:
  void Initialize() override;

 private:
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
};

void TestSuiteNoAtExit::Initialize() {
  // Run TestSuite::Initialize first so that logging is initialized.
  base::TestSuite::Initialize();

#if BUILDFLAG(IS_ANDROID)
  media::MediaCodecBridgeImpl::SetupCallbackHandlerForTesting();
#endif

  // Run this here instead of main() to ensure an AtExitManager is already
  // present.
  media::InitializeMediaLibrary();
  media::SetUpFakeLocalizedStrings();

  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);
}

int main(int argc, char** argv) {
  mojo::core::Init();
  TestSuiteNoAtExit test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&TestSuiteNoAtExit::Run, base::Unretained(&test_suite)));
}
