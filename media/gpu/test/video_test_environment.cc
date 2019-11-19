// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_test_environment.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "media/gpu/buildflags.h"
#include "mojo/core/embedder/embedder.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_gpu_test_helper.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace media {
namespace test {

VideoTestEnvironment::VideoTestEnvironment() {
  // Using shared memory requires mojo to be initialized (crbug.com/849207).
  mojo::core::Init();

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  if (!logging::InitLogging(settings))
    ADD_FAILURE();

  // Setting up a task environment will create a task runner for the current
  // thread and allow posting tasks to other threads. This is required for video
  // tests to function correctly.
  TestTimeouts::Initialize();
  task_environment_ = std::make_unique<base::test::TaskEnvironment>(
      base::test::TaskEnvironment::MainThreadType::UI);

  // Perform all static initialization that is required when running video
  // decoders in a test environment.
#if BUILDFLAG(USE_VAAPI)
  media::VaapiWrapper::PreSandboxInitialization();
#endif

#if defined(USE_OZONE)
  // Initialize Ozone. This is necessary to gain access to the GPU for hardware
  // video decode acceleration.
  LOG(WARNING) << "Initializing Ozone Platform...\n"
                  "If this hangs indefinitely please call 'stop ui' first!";
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForUI(params);
  ui::OzonePlatform::InitializeForGPU(params);

  // Initialize the Ozone GPU helper. If this is not done an error will occur:
  // "Check failed: drm. No devices available for buffer allocation."
  // Note: If a task environment is not set up initialization will hang
  // indefinitely here.
  gpu_helper_.reset(new ui::OzoneGpuTestHelper());
  gpu_helper_->Initialize(base::ThreadTaskRunnerHandle::Get());
#endif
}

VideoTestEnvironment::~VideoTestEnvironment() = default;

void VideoTestEnvironment::TearDown() {
  // Some implementations (like VideoDecoder) might be destroyed on a different
  // thread from the thread that the client releases it on. Call RunUntilIdle()
  // to ensure this kind of destruction is finished before |task_environment_|
  // is destroyed.
  task_environment_->RunUntilIdle();
}

base::FilePath VideoTestEnvironment::GetTestOutputFilePath() const {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  base::FilePath::StringType test_name;
  base::FilePath::StringType test_suite_name;
#if defined(OS_WIN)
  // On Windows the default file path string type is UTF16. Since the test name
  // is always returned in UTF8 we need to do a conversion here.
  test_name = base::UTF8ToUTF16(test_info->name());
  test_suite_name = base::UTF8ToUTF16(test_info->test_suite_name());
#else
  test_name = test_info->name();
  test_suite_name = test_info->test_suite_name();
#endif
  return base::FilePath(test_suite_name).Append(test_name);
}

}  // namespace test
}  // namespace media
