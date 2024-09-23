// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_test_environment.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "media/gpu/buildflags.h"
#include "mojo/core/embedder/embedder.h"

#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_utils.h"
#endif

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

namespace media {
namespace test {

VideoTestEnvironment::VideoTestEnvironment() : VideoTestEnvironment({}, {}) {}

VideoTestEnvironment::VideoTestEnvironment(
    const std::vector<base::test::FeatureRef>& enabled_features,
    const std::vector<base::test::FeatureRef>& disabled_features,
    const bool need_task_environment) {
  // Using shared memory requires mojo to be initialized (crbug.com/849207).
  mojo::core::Init();

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  if (!logging::InitLogging(settings))
    ADD_FAILURE();

  // Setting up a task environment will create a task runner for the current
  // thread and allow posting tasks to other threads. This is required for
  // video tests to function correctly.
  //
  // If |need_task_environment| is not set, the caller is responsible
  // for creating a TaskEnvironment.
  if (need_task_environment) {
    TestTimeouts::Initialize();
    task_environment_ = std::make_unique<base::test::TaskEnvironment>(
// Not sure why on CrOS this needs to be UI thread type? On Windows we use
// the default type.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
        base::test::TaskEnvironment::MainThreadType::UI
#else
        base::test::TaskEnvironment::MainThreadType::DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
    );

    at_exit_manager_ = std::make_unique<base::AtExitManager>();
  }

  // Initialize features. Since some of them can be for VA-API, it is necessary
  // to initialize them before calling VaapiWrapper::PreSandboxInitialization().
  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

#if BUILDFLAG(USE_VAAPI)
  media::VaapiWrapper::PreSandboxInitialization(
      /*allow_disabling_global_lock=*/true);
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
#if BUILDFLAG(IS_WIN)
  test_name =
      base::FilePath::FromASCII(base::StringPrintf("%s", test_info->name()))
          .value();
  test_suite_name = base::FilePath::FromASCII(
                        base::StringPrintf("%s", test_info->test_suite_name()))
                        .value();
#else
  test_name = test_info->name();
  test_suite_name = test_info->test_suite_name();
#endif  // BUILDFLAG(IS_WIN)
  return base::FilePath(test_suite_name).Append(test_name);
}

bool VideoTestEnvironment::IsV4L2VirtualDriver() const {
#if BUILDFLAG(USE_V4L2_CODEC)
  return IsVislDriver();
#else
  return false;
#endif
}

}  // namespace test
}  // namespace media
