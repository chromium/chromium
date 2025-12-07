// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_test_suite.h"

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/sanitizer_buildflags.h"
#include "build/build_config.h"
#include "content/browser/webui/content_web_ui_configs.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/setup_field_trials.h"
#include "gpu/ipc/test_gpu_thread_holder.h"
#include "media/base/media.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/dpi.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/test/mock_chrome_application_mac.h"
#endif

namespace content {

ContentTestSuite::ContentTestSuite(int argc, char** argv)
    : ContentTestSuiteBase(argc, argv) {
  // --------------------------------------------------
  // Failing tests on windows:
  // -DelegatedFrameHostTest.NoCopyOutputRequestWithNoValidSurface
  // -EmbeddedFrameSinkProviderImplTest.*
  // -RendererSandboxSettings/RendererFeatureSandboxWinTest.RendererGeneratedPolicyTest/0
  // TODO(40105939): Enable field trials on windows.
  // --------------------------------------------------
  // On Android, `content_unittests` fails during `--gtest-list-tests` with
  // no debug information.
  // TODO(40105939): Enable field trials on android.
  // --------------------------------------------------
  // Several memory safety issues were detected when using a sanitizer in
  // 16 different tests: Invalid downcast, type confusion and normal failure.
  // TODO(40105939): Investigate, and enable field trials with sanitizers.
  // --------------------------------------------------
  // Note that this could be moved to `content::UnitTestTestSuite` or
  // `base::TestSuite` at some point to target all of the unittests, instead of
  // just the `content_unittests`.
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(USING_SANITIZER)
  SetupFieldTrials();

  // Some field trial features are failing tests, so they are disabled here.
  // This was done as a temporary measure to start enabling field trials in
  // general in content_unittests.
  std::vector<std::string> disabled_field_trial;
  auto disable_field_trial = [&](std::string feature) {
    base::FeatureList* feature_list = base::FeatureList::GetInstance();
    // The user's command line take precedence.
    // This is important for developers to investigate why a feature causes
    // failures.
    if (feature_list->IsFeatureOverriddenFromCommandLine(feature)) {
      return;
    }

    // Do not revert a feature if it wasn't enabled via a field trial, e.g. in
    // branded builds.
    if (!feature_list->IsFeatureOverridden(feature)) {
      return;
    }

    CHECK(feature_list->GetEnabledFieldTrialByFeatureName(feature));
    disabled_field_trial.push_back(feature);
  };

  // TODO(447172722) Enable testing for this field trial feature.
  // Reproducer:AttributionManagerImplTest.SendReport_RecordsTimeFromLastNavigation_Failure
  disable_field_trial("AttributionReportNavigationBasedRetry");

  // TODO(447306905) Enable testing for this field trial feature.
  // Reproducer:BiddingAndAuctionSerializerTest.SerializeWithTooSmallRequestSize
  disable_field_trial("FledgeEnableSampleDebugReportOnCookieSetting");
  disable_field_trial("FledgeEnforceKAnonymity");
  disable_field_trial("FledgeSendDebugReportCooldownsToBandA");

  // TODO(447306422) Enable testing for this field trial feature.
  // Reproducer:InputRouterImplTest.AsyncTouchMoveAckedImmediately
  disable_field_trial("SendEmptyGestureScrollUpdate");

  // TODO(447305317) Enable testing for this field trial feature.
  // Reproducer:NavigationPolicyContainerBuilderTest.MHTMLSandboxFlags
  disable_field_trial("MHTML_Improvements");

  // TODO(447306273) Enable testing for this field trial feature.
  // Reproducer:FileSystemAccessFileHandleImplRequestPermissionTest.RequestWrite_Denied
  disable_field_trial("TrackEmptyRendererProcessesForReuse");

  // TODO(447305320) Enable testing for this field trial feature.
  // Reproducer:NavigatorTest.NoContent
  disable_field_trial("DeferSpeculativeRFHCreation");

  // TODO(447307242) Enable testing for this field trial feature.
  // Reproducer:PrerendererTest.RemoveRendererHostAfterCandidateRemoved
  disable_field_trial("LCPTimingPredictorPrerender2");

  // TODO(447306957) Enable testing for this field trial feature.
  // Reproducer:ServiceWorkerRaceNetworkRequestURLLoaderClientTest.NetworkError_AfterInitialResponse
  disable_field_trial(
      "ServiceWorkerStaticRouterRaceNetworkRequestPerformanceImprovement");

  // TODO(447307245) Enable testing for this field trial feature.
  // Reproducer:ParametrizedTests/PrefetchServiceTest.PrefetchQueueNotStuckWhenResettingRunningPrefetch/2
  disable_field_trial("Prerender2FallbackPrefetchSpecRules");

  revert_field_trial_features_.InitFromCommandLine(
      /*enable_features=*/{},
      /*disable_features=*/base::JoinString(disabled_field_trial, ","));
#endif
}

ContentTestSuite::~ContentTestSuite() = default;

void ContentTestSuite::Initialize() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool is_child_process = command_line->HasSwitch(switches::kTestChildProcess);
#if BUILDFLAG(IS_MAC)
  base::apple::ScopedNSAutoreleasePool autorelease_pool;
  // Initializing `NSApplication` before applying the sandbox profile in child
  // processes opens XPC connections that should be disallowed. We don't want or
  // need `NSApplication` in sandboxed processes anyway, so skip initializing.
  if (!is_child_process) {
    mock_cr_app::RegisterMockCrApp();
  }
#endif

#if BUILDFLAG(IS_WIN)
  display::win::SetDefaultDeviceScaleFactor(1.0f);
#endif

  InitializeResourceBundle();

  ForceInProcessNetworkService();

  ContentTestSuiteBase::Initialize();
  {
    ContentClient client;
    ContentTestSuiteBase::RegisterContentSchemes(&client);
  }
  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);

  RegisterPathProvider();
  media::InitializeMediaLibrary();
  // When running in a child process for Mac sandbox tests, the sandbox exists
  // to initialize GL, so don't do it here.
  if (!is_child_process) {
    gl::GLDisplay* display =
        gl::GLSurfaceTestSupport::InitializeNoExtensionsOneOff();
    auto* gpu_feature_info = gpu::GetTestGpuThreadHolder()->GetGpuFeatureInfo();
    gl::init::SetDisabledExtensionsPlatform(
        gpu_feature_info->disabled_extensions);
    gl::init::InitializeExtensionSettingsOneOffPlatform(display);
  }

  RegisterContentWebUIConfigs();
}

}  // namespace content
