// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/shared_image_pool.h"

#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class SharedImagePoolTest : public testing::Test {
 public:
  SharedImagePoolTest() = default;
  ~SharedImagePoolTest() override = default;

 protected:
  void SetUp() override {
    test_sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();
  }

  scoped_refptr<gpu::TestSharedImageInterface> test_sii_;
};

// Extending ClientImage for testing.
class TestClientImage : public ClientImage {
 public:
  explicit TestClientImage(scoped_refptr<ClientSharedImage> shared_image)
      : ClientImage(std::move(shared_image)) {
    static int i = 0;
    id = ++i;
  }

  int id = 0;

 protected:
  friend class base::RefCounted<TestClientImage>;
  ~TestClientImage() override = default;
};

// Class used for showcasing complex usage of SharedImagePool with additional
// metadata in ClientImage.
class ExtendedClientImage : public ClientImage {
 public:
  int extra_metadata = 0;

  explicit ExtendedClientImage(scoped_refptr<ClientSharedImage> shared_image)
      : ClientImage(std::move(shared_image)) {}

  void SetMetadata(int metadata) { extra_metadata = metadata; }

 protected:
  friend class base::RefCounted<ExtendedClientImage>;
  ~ExtendedClientImage() override = default;
};

// Test for verifying if shared image and creation sync token have been created.
TEST_F(SharedImagePoolTest, VerifyImage) {
  ImageInfo info = {
      gfx::Size(1920, 1080), viz::SinglePlaneFormat::kRGBA_8888, {}};
  auto pool = SharedImagePool<ClientImage>::Create(info, test_sii_);

  auto image = pool->GetImage();
  // Verify shared image is created.
  EXPECT_TRUE(image->GetSharedImage() != nullptr);
  // Verify the SyncToken has data.
  EXPECT_TRUE(image->GetSyncToken().HasData());
}

// Test for verifying releasing and recycling images in the pool.
TEST_F(SharedImagePoolTest, ReleaseAndRecycleImage) {
  ImageInfo info = {
      gfx::Size(1920, 1080), viz::SinglePlaneFormat::kRGBA_8888, {}};
  auto pool = SharedImagePool<TestClientImage>::Create(info, test_sii_);

  auto image1 = pool->GetImage();
  auto image1_id = image1->id;
  pool->ReleaseImage(std::move(image1));
  auto recycled_image = pool->GetImage();
  auto recycled_image_id = recycled_image->id;

  // Check if the recycled image is the same as the one released.
  EXPECT_EQ(recycled_image_id, image1_id);
}

// Test the pool's behavior when it reaches maximum capacity.
TEST_F(SharedImagePoolTest, MaxPoolSizeBehavior) {
  ImageInfo info = {
      gfx::Size(1024, 768), viz::SinglePlaneFormat::kRGBA_8888, {}};
  const auto max_pool_size = 1;
  auto pool =
      SharedImagePool<TestClientImage>::Create(info, test_sii_, max_pool_size);

  auto image1 = pool->GetImage();
  auto image2 = pool->GetImage();
  auto image2_id = image2->id;

  pool->ReleaseImage(std::move(image1));
  // This image should not be stored as the pool is full.
  pool->ReleaseImage(std::move(image2));

  auto retrieved_image = pool->GetImage();
  auto retrieved_image_id = retrieved_image->id;
  // The second image should not be the same as retrieved image.
  EXPECT_NE(retrieved_image_id, image2_id);
}

// Test for verifying maximum pool size behavior.
TEST_F(SharedImagePoolTest, MaxPoolSizeEnforcement) {
  ImageInfo info = {
      gfx::Size(1024, 768), viz::SinglePlaneFormat::kRGBA_8888, {}};
  const size_t max_pool_size = 1;
  auto pool =
      SharedImagePool<ClientImage>::Create(info, test_sii_, max_pool_size);

  auto image1 = pool->GetImage();
  auto image2 = pool->GetImage();
  pool->ReleaseImage(std::move(image1));

  // Should trigger destruction of one image.
  pool->ReleaseImage(std::move(image2));

  // Pool should not exceed max size.
  EXPECT_EQ(pool->GetPoolSizeForTesting(), max_pool_size);
}

// Test for verifying release sync token behaviour.
TEST_F(SharedImagePoolTest, TokenConsistencyOnReuse) {
  ImageInfo info = {
      gfx::Size(1920, 1080), viz::SinglePlaneFormat::kRGBA_8888, {}};
  auto pool = SharedImagePool<ClientImage>::Create(info, test_sii_);

  auto image = pool->GetImage();
  gpu::SyncToken release_token = test_sii_->GenUnverifiedSyncToken();
  image->SetReleaseSyncToken(release_token);
  pool->ReleaseImage(std::move(image));

  auto reused_image = pool->GetImage();
  EXPECT_EQ(reused_image->GetSyncToken(), release_token);
}

// Test for verifying release sync token behaviour.
TEST_F(SharedImagePoolTest, ProperTokenHandlingBeforeReuse) {
  ImageInfo info = {
      gfx::Size(800, 600), viz::SinglePlaneFormat::kRGBA_8888, {}};
  const size_t max_pool_size = 1;
  auto pool =
      SharedImagePool<ClientImage>::Create(info, test_sii_, max_pool_size);

  auto image1 = pool->GetImage();
  auto image2 = pool->GetImage();

  // |image1| will be cached in the pool.
  gpu::SyncToken release_token1 = test_sii_->GenUnverifiedSyncToken();
  image1->SetReleaseSyncToken(release_token1);
  pool->ReleaseImage(std::move(image1));

  // |image2| will be destroyed since the cache is full.
  gpu::SyncToken release_token2 = test_sii_->GenUnverifiedSyncToken();
  image2->SetReleaseSyncToken(release_token2);
  pool->ReleaseImage(std::move(image2));

  // |image3| will be re-used from the pool.
  auto image3 = pool->GetImage();
  EXPECT_EQ(image3->GetSyncToken(), release_token1);
  EXPECT_TRUE(image3->GetSyncToken().HasData());
}

// Test for showcasing complex client usage via ExtendedClientImage.
TEST_F(SharedImagePoolTest, ComplexClientUsage) {
  ImageInfo info = {
      gfx::Size(1280, 720), viz::SinglePlaneFormat::kRGBA_8888, {}};
  auto pool = SharedImagePool<ExtendedClientImage>::Create(info, test_sii_);

  scoped_refptr<ExtendedClientImage> extended_client_image = pool->GetImage();
  EXPECT_TRUE(extended_client_image);

  // Sets additional metadata.
  extended_client_image->SetMetadata(42);
  // Verify metadata is set and retrieved correctly
  EXPECT_EQ(extended_client_image->extra_metadata, 42);
  // Verify that the SyncToken within the image is valid.
  EXPECT_TRUE(extended_client_image->GetSyncToken().HasData());
}

// Test to ensure that images from a pool with one configuration are not reused
// in a pool with a different configuration.
TEST_F(SharedImagePoolTest, DiscardImageFromDifferentPool) {
  ImageInfo pool1_info = {
      gfx::Size(1920, 1080), viz::SinglePlaneFormat::kRGBA_8888, {}};
  ImageInfo pool2_info = {
      gfx::Size(800, 600), viz::SinglePlaneFormat::kRGBA_8888, {}};

  auto pool1 = SharedImagePool<ClientImage>::Create(pool1_info, test_sii_);
  auto pool2 =
      SharedImagePool<ClientImage>::Create(pool2_info, test_sii_.get());

  auto image_from_pool1 = pool1->GetImage();
  EXPECT_TRUE(image_from_pool1);

  // Attempt to release this image into pool2 which has different ImageInfo.
  pool2->ReleaseImage(std::move(image_from_pool1));

  // Check that pool2 has not accepted the image from pool1 due to mismatched
  // ImageInfo.
  EXPECT_EQ(pool2->GetPoolSizeForTesting(), size_t(0));
}

// Test Reconfigure method of SharedImagePool.
TEST_F(SharedImagePoolTest, ReconfigurePool) {
  // Initial ImageInfo.
  ImageInfo initial_info = {
      gfx::Size(1920, 1080), viz::SinglePlaneFormat::kRGBA_8888, {}};

  // Create the pool with an initial configuration.
  auto pool = SharedImagePool<ClientImage>::Create(
      initial_info, test_sii_.get(), /*max_pool_size=*/2);

  // Create a different ImageInfo.
  ImageInfo new_info = {
      gfx::Size(1280, 720), viz::SinglePlaneFormat::kBGRA_8888, {}};

  // Reconfigure with a different ImageInfo.
  pool->Reconfigure(new_info);

  // Verify that the pool was reconfigured.
  EXPECT_EQ(pool->GetImageInfo(), new_info);

  // The pool should now return images with the new configuration.
  auto image = pool->GetImage();
  ASSERT_TRUE(image);
  EXPECT_EQ(image->GetSharedImage()->size(), new_info.size);
  EXPECT_EQ(image->GetSharedImage()->format(), new_info.format);

  // Attempt to reconfigure with the same ImageInfo, which should be a no-op.
  pool->Reconfigure(new_info);

  // Ensure the ImageInfo remains unchanged.
  EXPECT_EQ(pool->GetImageInfo(), new_info);

  // Confirm that images created after reconfiguration still follow the new
  // configuration
  auto new_image = pool->GetImage();
  ASSERT_TRUE(new_image);
  EXPECT_EQ(new_image->GetSharedImage()->size(), new_info.size);
  EXPECT_EQ(new_image->GetSharedImage()->format(), new_info.format);
}

// Test for setting the release sync token in ClientImage.
TEST_F(SharedImagePoolTest, SetReleaseSyncToken) {
  ImageInfo info = {
      gfx::Size(1024, 768), viz::SinglePlaneFormat::kRGBA_8888, {}};
  auto pool = SharedImagePool<TestClientImage>::Create(info, test_sii_);

  // Create a new ClientImage object from the pool.
  auto client_image = pool->GetImage();

  // Verify that the client image was successfully created.
  EXPECT_TRUE(client_image != nullptr);

  // Create a dummy SyncToken to simulate release.
  SyncToken release_sync_token(gpu::CommandBufferNamespace::GPU_IO,
                               gpu::CommandBufferId::FromUnsafeValue(1), 12345);

  // Set the release SyncToken.
  client_image->SetReleaseSyncToken(release_sync_token);

  // Verify that the release SyncToken was set correctly.
  EXPECT_EQ(client_image->GetSyncToken(), release_sync_token);
}

}  // namespace gpu
