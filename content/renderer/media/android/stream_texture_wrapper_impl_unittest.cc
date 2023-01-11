// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/stream_texture_wrapper_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

namespace content {

class StreamTextureWrapperImplTest : public testing::Test {
 public:
  StreamTextureWrapperImplTest() {}

  StreamTextureWrapperImplTest(const StreamTextureWrapperImplTest&) = delete;
  StreamTextureWrapperImplTest& operator=(const StreamTextureWrapperImplTest&) =
      delete;

  // Necessary, or else GetSingleThreadTaskRunnerForTesting() fails.
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// This test's purpose is to make sure the StreamTextureWrapperImpl can properly
// be destroyed via StreamTextureWrapper::Deleter.
TEST_F(StreamTextureWrapperImplTest, ConstructionDestruction_ShouldSucceed) {
  media::ScopedStreamTextureWrapper stream_texture_wrapper =
      StreamTextureWrapperImpl::Create(
          false, nullptr,
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  // Since we provided a null factory, make sure that it also doesn't crash if
  // we try to initialize it.
  int result = 0;
  stream_texture_wrapper->Initialize(
      base::DoNothing(),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
      base::BindOnce(
          [](int* result_out, bool result) { *result_out = result ? 1 : 2; },
          &result));
  base::RunLoop().RunUntilIdle();
  // Should be called with false.
  EXPECT_EQ(result, 2);
}

}  // Content
