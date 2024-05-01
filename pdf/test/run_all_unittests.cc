// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/task_environment.h"
#include "base/test/test_suite.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "pdf/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/web/blink.h"
#include "tools/v8_context_snapshot/buildflags.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "v8/include/v8.h"

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#include "gin/v8_initializer.h"
#endif

namespace {

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
constexpr gin::V8SnapshotFileType kSnapshotType =
#if BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
    gin::V8SnapshotFileType::kWithAdditionalContext;
#else
    gin::V8SnapshotFileType::kDefault;
#endif  // BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
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
    blink::InitializeWithoutIsolateForTesting(
        platform_.get(), &binders, platform_->GetMainThreadScheduler());
    v8::Isolate* isolate = blink::CreateMainThreadIsolate();
    chrome_pdf::SetBlinkIsolate(isolate);
    InitializeResourceBundle();
  }

  void Shutdown() override {
    chrome_pdf::SetBlinkIsolate(nullptr);
    platform_.reset();
    ui::ResourceBundle::CleanupSharedInstance();
    base::TestSuite::Shutdown();
  }

 private:
  void InitializeResourceBundle() {
    ui::RegisterPathProvider();
    base::FilePath ui_test_pak_path =
        base::PathService::CheckedGet(ui::UI_TEST_PAK);
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

    base::FilePath pdf_tests_pak_path =
        base::PathService::CheckedGet(base::DIR_ASSETS);
    pdf_tests_pak_path =
        pdf_tests_pak_path.AppendASCII("pdf_tests_resources.pak");
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pdf_tests_pak_path, ui::kScaleFactorNone);
  }

  std::unique_ptr<BlinkPlatformForTesting> platform_;
};

}  // namespace

int main(int argc, char** argv) {
  PdfTestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&PdfTestSuite::Run, base::Unretained(&test_suite)));
}
