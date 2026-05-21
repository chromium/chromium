// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "media/renderers/win/media_foundation_source_wrapper.h"

#include <mfapi.h>

#include <memory>

#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

class MediaFoundationSourceWrapperTest : public testing::Test {
 public:
  MediaFoundationSourceWrapperTest() {
    source_wrapper_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

    null_media_log_ = std::make_unique<NullMediaLog>();

    MakeAndInitialize<MediaFoundationSourceWrapper>(
        &mf_source_wrapper_, &media_resource_, null_media_log_.get(),
        source_wrapper_task_runner_);
  }

  ~MediaFoundationSourceWrapperTest() override {
    mf_source_wrapper_.Reset();
    task_environment_.RunUntilIdle();
  }

 protected:
  ComPtr<MediaFoundationSourceWrapper> mf_source_wrapper_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> source_wrapper_task_runner_;
  testing::NiceMock<MockMediaResource> media_resource_;
  std::unique_ptr<MediaLog> null_media_log_;
};

// Initializes a MediaFoundationSourceWrapper inside a simulated task runner
// environment, then manually triggers its destruction from a background thread
// to verify that it successfully bounces the destruction back to the sequenced
// task runner.
TEST_F(MediaFoundationSourceWrapperTest, DestructionOnTaskRunner) {
  auto wrapper = mf_source_wrapper_;
  mf_source_wrapper_.Reset();

  base::WaitableEvent event;
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(
                     [](ComPtr<MediaFoundationSourceWrapper> wrapper,
                        base::WaitableEvent* event) {
                       wrapper.Reset();
                       event->Signal();
                     },
                     std::move(wrapper), &event));
  event.Wait();

  // Wait for the task runner to process the deletion.
  task_environment_.RunUntilIdle();
}

}  // namespace media
