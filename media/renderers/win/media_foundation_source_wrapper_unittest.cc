// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "media/renderers/win/media_foundation_source_wrapper.h"

#include <mfapi.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/win/media_foundation_cdm_proxy.h"
#include "media/base/win/mf_mocks.h"
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
  }

  ~MediaFoundationSourceWrapperTest() override {
    if (mf_source_wrapper_) {
      mf_source_wrapper_.Reset();
    }
    task_environment_.RunUntilIdle();
  }

 protected:
  ComPtr<MediaFoundationSourceWrapper> mf_source_wrapper_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> source_wrapper_task_runner_;
  std::unique_ptr<testing::StrictMock<MockDemuxerStream>> video_stream_;
  testing::NiceMock<MockMediaResource> media_resource_;
  std::unique_ptr<MediaLog> null_media_log_;
};

// Initializes a MediaFoundationSourceWrapper inside a simulated task runner
// environment, then manually triggers its destruction from a background thread
// to verify that it successfully bounces the destruction back to the sequenced
// task runner.
TEST_F(MediaFoundationSourceWrapperTest, DestructionOnTaskRunner) {
  MakeAndInitialize<MediaFoundationSourceWrapper>(
      &mf_source_wrapper_, &media_resource_, null_media_log_.get(),
      source_wrapper_task_runner_);
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

TEST_F(MediaFoundationSourceWrapperTest, HasCdmSetsProtectedAttribute) {
  // Setup a demuxer stream that begins as clear (unencrypted).
  video_stream_ =
      CreateMockDemuxerStream(DemuxerStream::VIDEO, /*encrypted=*/false);
  EXPECT_CALL(media_resource_, GetAllStreams())
      .WillRepeatedly(testing::Return(
          std::vector<raw_ptr<DemuxerStream>>{video_stream_.get()}));

  ComPtr<MediaFoundationSourceWrapper> wrapper;
  MakeAndInitialize<MediaFoundationSourceWrapper>(&wrapper, &media_resource_,
                                                  null_media_log_.get(),
                                                  source_wrapper_task_runner_,
                                                  /*has_cdm=*/true);

  ComPtr<IMFPresentationDescriptor> presentation_descriptor;
  EXPECT_TRUE(SUCCEEDED(
      wrapper->CreatePresentationDescriptor(&presentation_descriptor)));

  ComPtr<IMFStreamDescriptor> stream_descriptor;
  BOOL selected;
  EXPECT_TRUE(SUCCEEDED(presentation_descriptor->GetStreamDescriptorByIndex(
      0, &selected, &stream_descriptor)));

  // Validate that the CDM expectation correctly forced the protected stream
  // attribute.
  UINT32 is_protected = 0;
  EXPECT_TRUE(
      SUCCEEDED(stream_descriptor->GetUINT32(MF_SD_PROTECTED, &is_protected)));
  EXPECT_EQ(is_protected, 1u);

  // The Media Engine normally calls Shutdown() on the media source when it is
  // destroyed. We must call it manually here to break the circular COM
  // reference between the source and its streams so they can be freed.
  EXPECT_TRUE(SUCCEEDED(wrapper->Shutdown()));
  wrapper.Reset();

  // Clear the mock expectations so that the raw_ptr to the DemuxerStream is
  // released before the stream itself is destroyed.
  testing::Mock::VerifyAndClearExpectations(&media_resource_);
}

}  // namespace media
