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

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/gfx/linux/gbm_util.h"  // nogncheck
#endif                              // BUILDFLAG(IS_CHROMEOS)

class TestSuiteNoAtExit : public base::TestSuite {
 public:
  TestSuiteNoAtExit(int argc, char** argv) : TestSuite(argc, argv) {
#if BUILDFLAG(IS_CHROMEOS)
    // TODO(b/271455200): the FeatureList has not been initialized by this
    // point, so this call will always disable Intel media compression. We may
    // want to move this to a later point to be able to run media unit tests
    // with Intel media compression enabled.
    ui::EnsureIntelMediaCompressionEnvVarIsSet();
#endif
  }
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
