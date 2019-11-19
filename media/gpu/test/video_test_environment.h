// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains helper classes for video accelerator unittests.

#ifndef MEDIA_GPU_TEST_VIDEO_TEST_ENVIRONMENT_H_
#define MEDIA_GPU_TEST_VIDEO_TEST_ENVIRONMENT_H_

#include <memory>

#include "base/at_exit.h"
#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_OZONE)
namespace ui {
class OzoneGpuTestHelper;
}  // namespace ui
#endif

namespace base {
namespace test {
class TaskEnvironment;
}  // namespace test
}  // namespace base

namespace media {
namespace test {

class VideoTestEnvironment : public ::testing::Environment {
 public:
  VideoTestEnvironment();
  virtual ~VideoTestEnvironment();

  // ::testing::Environment implementation.
  // Tear down video test environment, called once for entire test run.
  void TearDown() override;

  // Get the name of the test output file path (testsuitename/testname).
  base::FilePath GetTestOutputFilePath() const;

 private:
  // An exit manager is required to run callbacks on shutdown.
  base::AtExitManager at_exit_manager;

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;

#if defined(USE_OZONE)
  std::unique_ptr<ui::OzoneGpuTestHelper> gpu_helper_;
#endif
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_TEST_ENVIRONMENT_H_
