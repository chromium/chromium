// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"

#include "base/functional/callback_helpers.h"
#include "base/test/null_task_runner.h"
#include "base/test/task_environment.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/test/test_gles2_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
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

scoped_refptr<StaticBitmapImage> CreateBitmap() {
  auto client_si = gpu::ClientSharedImage::CreateForTesting();

  return AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
      std::move(client_si), GenTestSyncToken(100), 0,
      SkImageInfo::MakeN32Premul(100, 100), GL_TEXTURE_2D, true,
      SharedGpuContext::ContextProviderWrapper(),
      base::PlatformThread::CurrentRef(),
      base::MakeRefCounted<base::NullTaskRunner>(), base::DoNothing(),
      /*supports_display_compositing=*/true, /*is_overlay_candidate=*/true);
}

class AcceleratedStaticBitmapImageTest : public Test {
 public:
  void SetUp() override {
    auto gl = std::make_unique<MockGLES2InterfaceWithSyncTokenSupport>();
    gl_ = gl.get();
    context_provider_ = viz::TestContextProvider::Create(std::move(gl));
    InitializeSharedGpuContextGLES2(context_provider_.get());
  }
  void TearDown() override {
    gl_ = nullptr;
    SharedGpuContext::Reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  raw_ptr<MockGLES2InterfaceWithSyncTokenSupport> gl_;
  scoped_refptr<viz::TestContextProvider> context_provider_;
};

TEST_F(AcceleratedStaticBitmapImageTest, SkImageCached) {
  auto bitmap = CreateBitmap();

  cc::PaintImage stored_image = bitmap->PaintImageForCurrentFrame();
  auto stored_image2 = bitmap->PaintImageForCurrentFrame();
  EXPECT_TRUE(stored_image.IsSameForTesting(stored_image2));
}

TEST_F(AcceleratedStaticBitmapImageTest, CopyToTextureSynchronization) {
  auto bitmap = CreateBitmap();

  MockGLES2InterfaceWithSyncTokenSupport destination_gl;

  testing::Mock::VerifyAndClearExpectations(gl_);
  testing::Mock::VerifyAndClearExpectations(&destination_gl);

  InSequence s;  // Indicate to gmock that order of EXPECT_CALLs is important

  // Anterior synchronization. Wait on the sync token for the mailbox on the
  // dest context.
  EXPECT_CALL(destination_gl, WaitSyncTokenCHROMIUM(Pointee(SyncTokenMatcher(
                                  bitmap->GetMailboxHolder().sync_token))));

  // Posterior synchronization. Generate a sync token on the destination context
  // to ensure mailbox is destroyed after the copy.
  const gpu::SyncToken sync_token2 = GenTestSyncToken(2);
  EXPECT_CALL(destination_gl, GenUnverifiedSyncTokenCHROMIUM(_))
      .WillOnce(SetArrayArgument<0>(
          sync_token2.GetConstData(),
          sync_token2.GetConstData() + sizeof(gpu::SyncToken)));

  gfx::Point dest_point(0, 0);
  gfx::Rect source_sub_rectangle(0, 0, 10, 10);
  ASSERT_TRUE(bitmap->CopyToTexture(
      &destination_gl, GL_TEXTURE_2D, 1 /*dest_texture_id*/,
      0 /*dest_texture_level*/, false /*unpack_premultiply_alpha*/,
      false /*unpack_flip_y*/, dest_point, source_sub_rectangle));

  testing::Mock::VerifyAndClearExpectations(&gl_);
  testing::Mock::VerifyAndClearExpectations(&destination_gl);

  // Final wait is postponed until destruction.
  EXPECT_EQ(bitmap->GetMailboxHolder().sync_token, sync_token2);
}

}  // namespace
}  // namespace blink
