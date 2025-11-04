// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"

#include "base/compiler_specific.h"
#include "base/functional/callback_helpers.h"
#include "base/test/null_task_runner.h"
#include "base/test/task_environment.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_raster_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {
namespace {

using testing::_;
using testing::ElementsAreArray;
using testing::InSequence;
using testing::MatcherCast;
using testing::Pointee;
using testing::SetArgPointee;
using testing::SetArrayArgument;
using testing::Test;

class MockGLES2InterfaceWithSyncTokenSupport : public viz::TestGLES2Interface {
 public:
  MOCK_METHOD1(GenUnverifiedSyncTokenCHROMIUM, void(GLbyte*));
  MOCK_METHOD1(WaitSyncTokenCHROMIUM, void(const GLbyte*));
};

GLbyte SyncTokenMatcher(const gpu::SyncToken& token) {
  return reinterpret_cast<const GLbyte*>(&token)[0];
}

gpu::SyncToken GenTestSyncToken(GLbyte id) {
  gpu::SyncToken token;
  token.Set(gpu::CommandBufferNamespace::GPU_IO,
            gpu::CommandBufferId::FromUnsafeValue(64), id);
  return token;
}

scoped_refptr<StaticBitmapImage> CreateBitmap(
    gpu::SharedImageUsageSet usage = gpu::SharedImageUsageSet()) {
  auto client_si = gpu::ClientSharedImage::CreateForTesting(usage);

  return AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
      std::move(client_si), GenTestSyncToken(100), kPremul_SkAlphaType,
      SharedGpuContext::ContextProviderWrapper(),
      base::PlatformThread::CurrentRef(),
      base::MakeRefCounted<base::NullTaskRunner>(), base::DoNothing());
}

class AcceleratedStaticBitmapImageTest : public Test {
 public:
  void SetUp() override {
    context_provider_ = viz::TestContextProvider::CreateRaster();
    InitializeSharedGpuContextRaster(context_provider_.get());
  }
  void TearDown() override {
    SharedGpuContext::Reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<viz::TestContextProvider> context_provider_;
};

TEST_F(AcceleratedStaticBitmapImageTest, SkImageCached) {
  auto bitmap = CreateBitmap(gpu::SHARED_IMAGE_USAGE_RASTER_READ);

  cc::PaintImage stored_image = bitmap->PaintImageForCurrentFrame();
  auto stored_image2 = bitmap->PaintImageForCurrentFrame();
  EXPECT_TRUE(stored_image.IsSameForTesting(stored_image2));
}

TEST_F(AcceleratedStaticBitmapImageTest, CopyToTextureSynchronization) {
  auto bitmap = CreateBitmap(gpu::SHARED_IMAGE_USAGE_GLES2_READ |
                             gpu::SHARED_IMAGE_USAGE_GLES2_WRITE);

  MockGLES2InterfaceWithSyncTokenSupport destination_gl;

  testing::Mock::VerifyAndClearExpectations(&destination_gl);

  InSequence s;  // Indicate to gmock that order of EXPECT_CALLs is important

  // Anterior synchronization. Wait on the sync token for the mailbox on the
  // dest context.
  EXPECT_CALL(
      destination_gl,
      WaitSyncTokenCHROMIUM(Pointee(SyncTokenMatcher(bitmap->GetSyncToken()))))
      .Times(testing::Between(1, 2));

  // Posterior synchronization. Generate a sync token on the destination context
  // to ensure mailbox is destroyed after the copy.
  const gpu::SyncToken sync_token2 = GenTestSyncToken(2);
  EXPECT_CALL(destination_gl, GenUnverifiedSyncTokenCHROMIUM(_))
      .WillOnce(SetArrayArgument<0>(
          sync_token2.GetConstData(),
          UNSAFE_TODO(sync_token2.GetConstData() + sizeof(gpu::SyncToken))));

  gfx::Point dest_point(0, 0);
  gfx::Rect source_sub_rectangle(0, 0, 10, 10);
  ASSERT_TRUE(bitmap->CopyToTexture(
      &destination_gl, GL_TEXTURE_2D, /*dest_texture_id=*/1,
      /*dest_level=*/0, kUnpremul_SkAlphaType, kTopLeft_GrSurfaceOrigin,
      dest_point, source_sub_rectangle));

  testing::Mock::VerifyAndClearExpectations(&destination_gl);

  // Final wait is postponed until destruction.
  EXPECT_EQ(bitmap->GetSyncToken(), sync_token2);
}

}  // namespace
}  // namespace blink
