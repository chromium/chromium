// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_test_suite_base.h"

#include <memory>

#include "base/check.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "content/browser/gpu/gpu_main_thread_factory.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_host/utility_process_host.h"
#include "content/common/url_schemes.h"
#include "content/gpu/in_process_gpu_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/in_process_renderer_thread.h"
#include "content/utility/in_process_utility_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/v8_context_snapshot/buildflags.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#include "gin/v8_initializer.h"  // nogncheck
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/android/locale_utils.h"
#include "ui/base/resource/resource_bundle_android.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <tuple>

#include "ui/accessibility/platform/ax_platform_node_win.h"
#else
#include "ui/accessibility/platform/ax_platform_node_base.h"
#endif

namespace content {

namespace {
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
constexpr gin::V8SnapshotFileType kSnapshotType =
    gin::V8SnapshotFileType::kWithAdditionalContext;
#else
constexpr gin::V8SnapshotFileType kSnapshotType =
    gin::V8SnapshotFileType::kDefault;
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

// Checks that there are no leaked platform nodes of any type after each test.
class CheckForLeakedAxPlatformNodes : public testing::EmptyTestEventListener {
 public:
  void OnTestEnd(const testing::TestInfo& test_info) override {
    // Dear reader: If your test is failing here, it is because the test is
    // either leaking UX objects (e.g., Views, Widgets, etc) or is using
    // accessibility APIs that are leaking AXPlatformNode instances.
#if BUILDFLAG(IS_WIN)
    auto [instance_count, dormant_count, live_count, ghost_count] =
        ui::AXPlatformNodeWin::ResetCountsForTesting();
    EXPECT_EQ(ghost_count, 0U)
        << "This test is leaking COM interface references. If the test is not "
           "explicitly using facilities such as IAccessible, then you may have "
           "just found a bug in ui/accessibility/platform. Contact a member of "
           "ui/accessibility/OWNERS for guidance.";
#elif BUILDFLAG(IS_LINUX)
    // The `AuraLinuxApplication` singleton is never destroyed.
    // TODO(accessibility): Consider moving it into AXPlatform in some way so
    // that it can be properly destroyed at shutdown.
    // TODO(accessibility): Investigate platform node leaks on Linux; see
    // https://crrev.com/c/chromium/src/+/6316732?checksPatchset=10&tab=checks.
    size_t instance_count = 0;
#else
    size_t instance_count =
        ui::AXPlatformNodeBase::ResetInstanceCountForTesting();
#endif
    EXPECT_EQ(instance_count, 0U)
        << "This test is leaking UX objects (e.g., Views or Widgets). This is "
           "often caused by ownership issues with Widgets and NativeWidgets";
  }
};

}  // namespace

ContentTestSuiteBase::ContentTestSuiteBase(int argc, char** argv)
    : base::TestSuite(argc, argv) {
}

void ContentTestSuiteBase::Initialize() {
  base::TestSuite::Initialize();
  auto& listeners = testing::UnitTest::GetInstance()->listeners();
  listeners.Append(std::make_unique<SkipManualTests>().release());
  listeners.Append(new CheckForLeakedAxPlatformNodes());

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  gin::V8Initializer::LoadV8Snapshot(kSnapshotType);
#endif
}

void ContentTestSuiteBase::RegisterContentSchemes(
    ContentClient* content_client) {
  // In death tests UnitTestTestSuite::Run() (which ran before this) will have
  // already set the clients. So reset them back.
  auto* old_client = GetContentClientForTesting();
  SetContentClient(content_client);
  content::RegisterContentSchemes();
  SetContentClient(old_client);
}

void ContentTestSuiteBase::ReRegisterContentSchemes() {
  content::ReRegisterContentSchemesForTests();
}

void ContentTestSuiteBase::RegisterInProcessThreads() {
  UtilityProcessHost::RegisterUtilityMainThreadFactory(
      CreateInProcessUtilityThread);
  RenderProcessHostImpl::RegisterRendererMainThreadFactory(
      CreateInProcessRendererThread);
  content::RegisterGpuMainThreadFactory(CreateInProcessGpuThread);
}

void ContentTestSuiteBase::InitializeResourceBundle() {
  base::FilePath content_shell_pack_path;

#if BUILDFLAG(IS_ANDROID)
  // on Android all pak files are inside the paks folder.
  CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA,
                               &content_shell_pack_path));
  content_shell_pack_path =
      content_shell_pack_path.Append(FILE_PATH_LITERAL("paks"));
#else
  CHECK(base::PathService::Get(base::DIR_ASSETS, &content_shell_pack_path));
#endif  // BUILDFLAG(IS_ANDROID)

  // Add the content_shell main pak file.
  content_shell_pack_path =
      content_shell_pack_path.Append(FILE_PATH_LITERAL("content_shell.pak"));

  if (!ui::ResourceBundle::HasSharedInstance()) {
#if BUILDFLAG(IS_ANDROID)
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        base::android::GetDefaultLocaleString(), NULL,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);

    ui::LoadMainAndroidPackFile("assets/content_shell.pak",
                                content_shell_pack_path);
#else
    ui::ResourceBundle::InitSharedInstanceWithPakPath(content_shell_pack_path);
#endif
  }
}

}  // namespace content
