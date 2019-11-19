// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
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

class MockGLES2InterfaceWithSyncTokenSupport : public FakeGLES2Interface {
 public:
  MOCK_METHOD1(GenUnverifiedSyncTokenCHROMIUM, void(GLbyte*));
  MOCK_METHOD1(WaitSyncTokenCHROMIUM, void(const GLbyte*));
};

gpu::SyncToken GenTestSyncToken(GLbyte id) {
  gpu::SyncToken token;
  // Store id in the first byte
  reinterpret_cast<GLbyte*>(&token)[0] = id;
  return token;
}

GLbyte SyncTokenMatcher(const gpu::SyncToken& token) {
  return reinterpret_cast<const GLbyte*>(&token)[0];
}

class AcceleratedStaticBitmapImageTest : public Test {
 public:
  void SetUp() override {
    auto factory = [](MockGLES2InterfaceWithSyncTokenSupport* gl,
                      bool* gpu_compositing_disabled)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      *gpu_compositing_disabled = false;
      return std::make_unique<FakeWebGraphicsContext3DProvider>(gl, nullptr);
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        WTF::BindRepeating(factory, WTF::Unretained(&gl_)));
  }
  void TearDown() override { SharedGpuContext::ResetForTesting(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  MockGLES2InterfaceWithSyncTokenSupport gl_;
};

TEST_F(AcceleratedStaticBitmapImageTest, NoTextureHolderThrashing) {
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  GrContext* gr = context_provider_wrapper->ContextProvider()->GetGrContext();
  SkImageInfo imageInfo = SkImageInfo::MakeN32Premul(100, 100);

  sk_sp<SkSurface> surface =
      SkSurface::MakeRenderTarget(gr, SkBudgeted::kNo, imageInfo);

  SkPaint paint;
  surface->getCanvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);

  sk_sp<SkImage> image = surface->makeImageSnapshot();
  scoped_refptr<AcceleratedStaticBitmapImage> bitmap =
      AcceleratedStaticBitmapImage::CreateFromSkImage(image,
                                                      context_provider_wrapper);

  sk_sp<SkImage> stored_image =
      bitmap->PaintImageForCurrentFrame().GetSkImage();
  EXPECT_EQ(stored_image.get(), image.get());

  bitmap->EnsureMailbox(kUnverifiedSyncToken, GL_LINEAR);

  // Verify that calling PaintImageForCurrentFrame does not swap out of mailbox
  // mode. It should use the cached original image instead.
  stored_image = bitmap->PaintImageForCurrentFrame().GetSkImage();

  EXPECT_EQ(stored_image.get(), image.get());
}

TEST_F(AcceleratedStaticBitmapImageTest, CopyToTextureSynchronization) {
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  GrContext* gr = context_provider_wrapper->ContextProvider()->GetGrContext();
  SkImageInfo imageInfo = SkImageInfo::MakeN32Premul(100, 100);
  sk_sp<SkSurface> surface =
      SkSurface::MakeRenderTarget(gr, SkBudgeted::kNo, imageInfo);

  sk_sp<SkImage> image = surface->makeImageSnapshot();
  scoped_refptr<AcceleratedStaticBitmapImage> bitmap =
      AcceleratedStaticBitmapImage::CreateFromSkImage(image,
                                                      context_provider_wrapper);

  MockGLES2InterfaceWithSyncTokenSupport destination_gl;

  testing::Mock::VerifyAndClearExpectations(&gl_);
  testing::Mock::VerifyAndClearExpectations(&destination_gl);

  InSequence s;  // Indicate to gmock that order of EXPECT_CALLs is important

  // Anterior synchronization
  const gpu::SyncToken sync_token1 = GenTestSyncToken(1);
  EXPECT_CALL(gl_, GenUnverifiedSyncTokenCHROMIUM(_))
      .WillOnce(SetArrayArgument<0>(
          sync_token1.GetConstData(),
          sync_token1.GetConstData() + sizeof(gpu::SyncToken)));
  EXPECT_CALL(destination_gl,
              WaitSyncTokenCHROMIUM(Pointee(SyncTokenMatcher(sync_token1))));

  // Posterior synchronization
  const gpu::SyncToken sync_token2 = GenTestSyncToken(2);
  EXPECT_CALL(destination_gl, GenUnverifiedSyncTokenCHROMIUM(_))
      .WillOnce(SetArrayArgument<0>(
          sync_token2.GetConstData(),
          sync_token2.GetConstData() + sizeof(gpu::SyncToken)));

  IntPoint dest_point(0, 0);
  IntRect source_sub_rectangle(0, 0, 10, 10);
  bitmap->CopyToTexture(
      &destination_gl, GL_TEXTURE_2D, 1 /*dest_texture_id*/,
      0 /*dest_texture_level*/, false /*unpack_premultiply_alpha*/,
      false /*unpack_flip_y*/, dest_point, source_sub_rectangle);

  testing::Mock::VerifyAndClearExpectations(&gl_);
  testing::Mock::VerifyAndClearExpectations(&destination_gl);

  // Note the following expectation is commented-out because the
  // MailboxTextureHolder destructor skips it when the texture ID is 0.
  // The ID is zero because skia detected that it is being used with a fake
  // context, so this problem can't be solved by just mocking GenTextures to
  // make it produce non-zero IDs.
  // TODO(junov): fix this!

  // Final wait is postponed until destruction.
  // EXPECT_CALL(gl_,
  // WaitSyncTokenCHROMIUM(Pointee(SyncTokenMatcher(sync_token2)))); bitmap =
  // nullptr;
}

}  // namespace
}  // namespace blink
