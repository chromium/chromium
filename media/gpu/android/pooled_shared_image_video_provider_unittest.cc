// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/pooled_shared_image_video_provider.h"

#include <list>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "gpu/command_buffer/service/abstract_texture_impl_shared_context_state.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "media/gpu/android/mock_shared_image_video_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;

namespace media {

class PooledSharedImageVideoProviderTest : public testing::Test {
 public:
  class MockGpuHelper : public PooledSharedImageVideoProvider::GpuHelper {
   public:
    MockGpuHelper(gpu::SyncToken* sync_token_out)
        : sync_token_out_(sync_token_out) {}

    // PooledSharedImageVideoProvider::GpuHelper
    void OnImageReturned(const gpu::SyncToken& sync_token,
                         scoped_refptr<CodecImageHolder> codec_image_holder,
                         base::OnceClosure cb) override {
      *sync_token_out_ = sync_token;

      // Run the output cb.
      std::move(cb).Run();
    }

   private:
    gpu::SyncToken* sync_token_out_ = nullptr;
  };

  PooledSharedImageVideoProviderTest() = default;

  void SetUp() override {
    task_runner_ = base::SequencedTaskRunnerHandle::Get();
    base::SequenceBound<MockGpuHelper> mock_gpu_helper(task_runner_,
                                                       &sync_token_);

    std::unique_ptr<MockSharedImageVideoProvider> mock_provider =
        std::make_unique<MockSharedImageVideoProvider>();
    mock_provider_raw_ = mock_provider.get();

    provider_ = base::WrapUnique(new PooledSharedImageVideoProvider(
        std::move(mock_gpu_helper), std::move(mock_provider)));
  }

  // Return an ImageReadyCB that saves the ImageRecord in |image_records_|.
  SharedImageVideoProvider::ImageReadyCB SaveImageRecordCB() {
    return base::BindOnce(
        [](std::list<SharedImageVideoProvider::ImageRecord>* output_list,
           SharedImageVideoProvider::ImageRecord record) {
          output_list->push_back(std::move(record));
        },
        &image_records_);
  }

  // Request an image from |provier_|, which we expect will call through to
  // |mock_provider_raw_|.  Have |mock_provider_raw_| return an image, too.
  void RequestAndProvideImage(const SharedImageVideoProvider::ImageSpec& spec) {
    EXPECT_CALL(*mock_provider_raw_, MockRequestImage()).Times(1);
    provider_->RequestImage(SaveImageRecordCB(), spec, texture_owner_);
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(mock_provider_raw_);
    mock_provider_raw_->ProvideOneRequestedImage();
  }

  base::test::TaskEnvironment task_environment_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Provider under test.
  std::unique_ptr<PooledSharedImageVideoProvider> provider_;

  MockSharedImageVideoProvider* mock_provider_raw_ = nullptr;
  gpu::SyncToken sync_token_;

  scoped_refptr<gpu::TextureOwner> texture_owner_;

  // Image records that we've received from |provider|, via SaveImageRecordCB().
  std::list<SharedImageVideoProvider::ImageRecord> image_records_;
};

TEST_F(PooledSharedImageVideoProviderTest, InitializeForwardsGpuCallback) {
  bool was_called = false;
  auto gpu_init_cb = base::BindOnce(
      [](bool* flag, scoped_refptr<gpu::SharedContextState>) { *flag = true; },
      &was_called);
  provider_->Initialize(std::move(gpu_init_cb));
  std::move(mock_provider_raw_->gpu_init_cb_).Run(nullptr);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(was_called);
}

TEST_F(PooledSharedImageVideoProviderTest, RequestImageRequestsMultipleImages) {
  // Test the RequestImage will keep requesting images from the underlying
  // provider as long as we don't return any.
  SharedImageVideoProvider::ImageSpec spec(gfx::Size(1, 1), 0u);
  RequestAndProvideImage(spec);
  RequestAndProvideImage(spec);
  RequestAndProvideImage(spec);
  EXPECT_EQ(image_records_.size(), 3u);
}

TEST_F(PooledSharedImageVideoProviderTest, ReleasingAnImageForwardsSyncToken) {
  // Calling the release callback on an image should forward the sync token to
  // our gpu helper.
  SharedImageVideoProvider::ImageSpec spec(gfx::Size(1, 1), 0u);
  RequestAndProvideImage(spec);

  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                            gpu::CommandBufferIdFromChannelAndRoute(2, 3), 4);
  std::move(image_records_.back().release_cb).Run(sync_token);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(sync_token, sync_token_);
}

TEST_F(PooledSharedImageVideoProviderTest,
       ReleasingAnImageDoesntRunUnderlyingReleaseCallback) {
  // Verify that releasing an image doesn't call the underlying release callback
  // on it.  Presumably, it should be sent back to the pool instead.
  SharedImageVideoProvider::ImageSpec spec(gfx::Size(1, 1), 0u);
  RequestAndProvideImage(spec);

  // Release the image.
  image_records_.pop_back();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(mock_provider_raw_->num_release_callbacks_, 0);
}

TEST_F(PooledSharedImageVideoProviderTest, RequestImageReusesReturnedImages) {
  // Test the RequestImage will return images without requesting new ones, if
  // some have been returned to the pool.
  SharedImageVideoProvider::ImageSpec spec(gfx::Size(1, 1), 0u);
  // Request two images.
  RequestAndProvideImage(spec);
  RequestAndProvideImage(spec);
  EXPECT_EQ(image_records_.size(), 2u);
  // Now return one, and request another.
  image_records_.pop_back();
  // Let the release CB run.
  base::RunLoop().RunUntilIdle();

  // Shouldn't call MockRequestImage a third time.
  EXPECT_CALL(*mock_provider_raw_, MockRequestImage()).Times(0);
  provider_->RequestImage(SaveImageRecordCB(), spec, texture_owner_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(image_records_.size(), 2u);
}

TEST_F(PooledSharedImageVideoProviderTest,
       DeletingProviderWithOutstandingImagesDoesntCrash) {
  // Destroying |provider_| with outstanding images shouldn't break anything.
  SharedImageVideoProvider::ImageSpec spec(gfx::Size(1, 1), 0u);
  provider_->RequestImage(SaveImageRecordCB(), spec, texture_owner_);
  base::RunLoop().RunUntilIdle();
  provider_.reset();
  base::RunLoop().RunUntilIdle();
  // Shouldn't crash.
}

TEST_F(PooledSharedImageVideoProviderTest,
       ReturnedImagesAreReleasedAfterSpecChange) {
  // When we change the ImageSpec, old images should be released on the
  // underlying provider as they are returned.
  SharedImageVideoProvider::ImageSpec spec_1(gfx::Size(1, 1), 0u);
  SharedImageVideoProvider::ImageSpec spec_2(gfx::Size(1, 2), 0u);
  RequestAndProvideImage(spec_1);
  RequestAndProvideImage(spec_2);

  EXPECT_EQ(mock_provider_raw_->num_release_callbacks_, 0);

  // Return image 1, and it should run the underlying release callback since it
  // doesn't match the pool spec.
  image_records_.pop_front();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mock_provider_raw_->num_release_callbacks_, 1);

  // Returning image 2 should not, since it should be put into the pool.
  image_records_.pop_front();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mock_provider_raw_->num_release_callbacks_, 1);
}

TEST_F(PooledSharedImageVideoProviderTest, SizeChangeEmptiesPool) {
  // Verify that a size change in the ImageSpec causes the pool to be emptied.
  SharedImageVideoProvider::ImageSpec spec_1(gfx::Size(1, 1), 0u);
  SharedImageVideoProvider::ImageSpec spec_2(gfx::Size(1, 2), 0u);

  // Request an image with |spec_1| and release it, to send it to the pool.
  RequestAndProvideImage(spec_1);
  image_records_.pop_front();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mock_provider_raw_->num_release_callbacks_, 0);

  // Request an image with |spec_2|, which should release the first one.
  RequestAndProvideImage(spec_2);
  EXPECT_EQ(mock_provider_raw_->num_release_callbacks_, 1);
}

TEST_F(PooledSharedImageVideoProviderTest, GenerationIdChangeEmptiesPool) {
  // Verify that a change in the generation id causes the pool to be emptied.
  SharedImageVideoProvider::ImageSpec spec_1(gfx::Size(1, 1), 0u);
  SharedImageVideoProvider::ImageSpec spec_2(gfx::Size(1, 1), 1u);
  RequestAndProvideImage(spec_1);
  image_records_.pop_front();
  base::RunLoop().RunUntilIdle();
  RequestAndProvideImage(spec_2);
  EXPECT_EQ(mock_provider_raw_->num_release_callbacks_, 1);
}

TEST_F(PooledSharedImageVideoProviderTest, InFlightSpecChangeProvidesImage) {
  // If we change the ImageSpec between requesting and receiving an image from
  // the provider, it should still provide the image to the requestor.
  SharedImageVideoProvider::ImageSpec spec_1(gfx::Size(1, 1), 0u);
  SharedImageVideoProvider::ImageSpec spec_2(gfx::Size(1, 1), 1u);

  // Request both images before providing either.
  EXPECT_CALL(*mock_provider_raw_, MockRequestImage()).Times(2);
  provider_->RequestImage(SaveImageRecordCB(), spec_1, texture_owner_);
  provider_->RequestImage(SaveImageRecordCB(), spec_2, texture_owner_);
  base::RunLoop().RunUntilIdle();

  // Provide the |spec_1| image.  Nothing should be released since it should
  // fulfill the first request.
  mock_provider_raw_->ProvideOneRequestedImage();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mock_provider_raw_->num_release_callbacks_, 0);
  EXPECT_EQ(image_records_.size(), 1u);

  // Provide the |spec_2| image, which should also be provided to us.
  mock_provider_raw_->ProvideOneRequestedImage();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mock_provider_raw_->num_release_callbacks_, 0);
  EXPECT_EQ(image_records_.size(), 2u);

  // Drop the |spec_1| image, which should be released rather than added back to
  // the pool.
  image_records_.pop_front();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mock_provider_raw_->num_release_callbacks_, 1);

  // Drop the |spec_2| image, which should be pooled rather than released.
  image_records_.pop_front();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mock_provider_raw_->num_release_callbacks_, 1);
}

}  // namespace media
