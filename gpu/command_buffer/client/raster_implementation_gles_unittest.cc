// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/raster_implementation_gles.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/image_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/color_space.h"

using testing::_;
using testing::Eq;
using testing::Gt;
using testing::Le;
using testing::Pointee;
using testing::Return;
using testing::SetArgPointee;
using testing::StrEq;

namespace gpu {
namespace raster {

class RasterMockGLES2Interface : public gles2::GLES2InterfaceStub {
 public:
  // Command buffer Flush / Finish.
  MOCK_METHOD0(Finish, void());
  MOCK_METHOD0(Flush, void());
  MOCK_METHOD0(OrderingBarrierCHROMIUM, void());

  // SyncTokens.
  MOCK_METHOD1(GenSyncTokenCHROMIUM, void(GLbyte* sync_token));
  MOCK_METHOD1(GenUnverifiedSyncTokenCHROMIUM, void(GLbyte* sync_token));
  MOCK_METHOD2(VerifySyncTokensCHROMIUM,
               void(GLbyte** sync_tokens, GLsizei count));
  MOCK_METHOD1(WaitSyncTokenCHROMIUM, void(const GLbyte* sync_token));
  MOCK_METHOD0(ShallowFlushCHROMIUM, void());

  // Command buffer state.
  MOCK_METHOD0(GetError, GLenum());
  MOCK_METHOD0(GetGraphicsResetStatusKHR, GLenum());
  MOCK_METHOD2(GetIntegerv, void(GLenum pname, GLint* params));
  MOCK_METHOD2(LoseContextCHROMIUM, void(GLenum current, GLenum other));

  // Queries:
  // - GL_COMMANDS_ISSUED_CHROMIUM
  // - GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM
  // - GL_COMMANDS_COMPLETED_CHROMIUM
  MOCK_METHOD2(GenQueriesEXT, void(GLsizei n, GLuint* queries));
  MOCK_METHOD2(DeleteQueriesEXT, void(GLsizei n, const GLuint* queries));
  MOCK_METHOD2(BeginQueryEXT, void(GLenum target, GLuint id));
  MOCK_METHOD1(EndQueryEXT, void(GLenum target));
  MOCK_METHOD2(QueryCounterEXT, void(GLuint id, GLenum target));
  MOCK_METHOD3(GetQueryObjectuivEXT,
               void(GLuint id, GLenum pname, GLuint* params));
  MOCK_METHOD3(GetQueryObjectui64vEXT,
               void(GLuint id, GLenum pname, GLuint64* params));

  // Texture objects.
  MOCK_METHOD2(GenTextures, void(GLsizei n, GLuint* textures));
  MOCK_METHOD2(DeleteTextures, void(GLsizei n, const GLuint* textures));
  MOCK_METHOD2(BindTexture, void(GLenum target, GLuint texture));
  MOCK_METHOD1(ActiveTexture, void(GLenum texture));
  MOCK_METHOD1(GenerateMipmap, void(GLenum target));
  MOCK_METHOD3(TexParameteri, void(GLenum target, GLenum pname, GLint param));

  // Mailboxes.
  MOCK_METHOD1(CreateAndTexStorage2DSharedImageCHROMIUM,
               GLuint(const GLbyte* mailbox));
  MOCK_METHOD2(BeginSharedImageAccessDirectCHROMIUM,
               void(GLuint texture, GLenum mode));
  MOCK_METHOD1(EndSharedImageAccessDirectCHROMIUM, void(GLuint texture));

  // Texture allocation and copying.
  MOCK_METHOD9(TexImage2D,
               void(GLenum target,
                    GLint level,
                    GLint internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const void* pixels));
  MOCK_METHOD9(TexSubImage2D,
               void(GLenum target,
                    GLint level,
                    GLint xoffset,
                    GLint yoffset,
                    GLsizei width,
                    GLsizei height,
                    GLenum format,
                    GLenum type,
                    const void* pixels));
  MOCK_METHOD8(CompressedTexImage2D,
               void(GLenum target,
                    GLint level,
                    GLenum internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLsizei imageSize,
                    const void* data));
  MOCK_METHOD5(TexStorage2DEXT,
               void(GLenum target,
                    GLsizei levels,
                    GLenum internalFormat,
                    GLsizei width,
                    GLsizei height));

  MOCK_METHOD2(PixelStorei, void(GLenum pname, GLint param));
  MOCK_METHOD2(TraceBeginCHROMIUM,
               void(const char* category_name, const char* trace_name));
  MOCK_METHOD0(TraceEndCHROMIUM, void());
};

class ContextSupportStub : public ContextSupport {
 public:
  ~ContextSupportStub() override = default;

  void FlushPendingWork() override {}
  void SignalSyncToken(const SyncToken& sync_token,
                       base::OnceClosure callback) override {}
  bool IsSyncTokenSignaled(const SyncToken& sync_token) override {
    return false;
  }
  void SignalQuery(uint32_t query, base::OnceClosure callback) override {}
  void GetGpuFence(uint32_t gpu_fence_id,
                   base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>
                       callback) override {}
  void SetAggressivelyFreeResources(bool aggressively_free_resources) override {
  }

  uint64_t ShareGroupTracingGUID() const override { return 0; }
  void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char*, int32_t)> callback) override {}
  bool ThreadSafeShallowLockDiscardableTexture(uint32_t texture_id) override {
    return true;
  }
  void CompleteLockDiscardableTexureOnContextThread(
      uint32_t texture_id) override {}
  bool ThreadsafeDiscardableTextureIsDeletedForTracing(
      uint32_t texture_id) override {
    return false;
  }
  void* MapTransferCacheEntry(uint32_t serialized_size) override {
    mapped_transfer_cache_entry_.reset(new char[serialized_size]);
    return mapped_transfer_cache_entry_.get();
  }
  void UnmapAndCreateTransferCacheEntry(uint32_t type, uint32_t id) override {
    mapped_transfer_cache_entry_.reset();
  }
  bool ThreadsafeLockTransferCacheEntry(uint32_t type, uint32_t id) override {
    return true;
  }
  void UnlockTransferCacheEntries(
      const std::vector<std::pair<uint32_t, uint32_t>>& entries) override {}
  void DeleteTransferCacheEntry(uint32_t type, uint32_t id) override {}
  unsigned int GetTransferBufferFreeSize() const override { return 0; }
  bool IsJpegDecodeAccelerationSupported() const override { return false; }
  bool IsWebPDecodeAccelerationSupported() const override { return false; }
  bool CanDecodeWithHardwareAcceleration(
      const cc::ImageHeaderMetadata* image_metadata) const override {
    return false;
  }
  bool HasGrContextSupport() const override { return false; }
  void SetGrContext(GrDirectContext* gr) override {}
  void WillCallGLFromSkia() override {}
  void DidCallGLFromSkia() override {}

 private:
  std::unique_ptr<char[]> mapped_transfer_cache_entry_;
};

class ImageProviderStub : public cc::ImageProvider {
 public:
  ~ImageProviderStub() override {}
  ScopedResult GetRasterContent(const cc::DrawImage& draw_image) override {
    return ScopedResult();
  }
};

class RasterImplementationGLESTest : public testing::Test {
 protected:
  RasterImplementationGLESTest() {}

  void SetUp() override {
    gl_ = std::make_unique<RasterMockGLES2Interface>();
    ri_ = std::make_unique<RasterImplementationGLES>(gl_.get(), &support_,
                                                     gpu::Capabilities());
  }

  void TearDown() override {}

  void ExpectBindTexture(GLenum target, GLuint texture_id) {
    if (bound_texture_ != texture_id) {
      bound_texture_ = texture_id;
      EXPECT_CALL(*gl_, BindTexture(target, texture_id)).Times(1);
    }
  }

  ContextSupportStub support_;
  std::unique_ptr<RasterMockGLES2Interface> gl_;
  std::unique_ptr<RasterImplementationGLES> ri_;

  GLuint bound_texture_ = 0;
};

TEST_F(RasterImplementationGLESTest, Finish) {
  EXPECT_CALL(*gl_, Finish()).Times(1);
  ri_->Finish();
}

TEST_F(RasterImplementationGLESTest, Flush) {
  EXPECT_CALL(*gl_, Flush()).Times(1);
  ri_->Flush();
}

TEST_F(RasterImplementationGLESTest, ShallowFlushCHROMIUM) {
  EXPECT_CALL(*gl_, ShallowFlushCHROMIUM()).Times(1);
  ri_->ShallowFlushCHROMIUM();
}

TEST_F(RasterImplementationGLESTest, OrderingBarrierCHROMIUM) {
  EXPECT_CALL(*gl_, OrderingBarrierCHROMIUM()).Times(1);
  ri_->OrderingBarrierCHROMIUM();
}

TEST_F(RasterImplementationGLESTest, GenUnverifiedSyncTokenCHROMIUM) {
  GLbyte sync_token_data[GL_SYNC_TOKEN_SIZE_CHROMIUM] = {};

  EXPECT_CALL(*gl_, GenUnverifiedSyncTokenCHROMIUM(sync_token_data)).Times(1);
  ri_->GenUnverifiedSyncTokenCHROMIUM(sync_token_data);
}

TEST_F(RasterImplementationGLESTest, VerifySyncTokensCHROMIUM) {
  const GLsizei kNumSyncTokens = 2;
  GLbyte sync_token_data[GL_SYNC_TOKEN_SIZE_CHROMIUM][kNumSyncTokens] = {};
  GLbyte* sync_tokens[2] = {sync_token_data[0], sync_token_data[1]};

  EXPECT_CALL(*gl_, VerifySyncTokensCHROMIUM(sync_tokens, kNumSyncTokens))
      .Times(1);
  ri_->VerifySyncTokensCHROMIUM(sync_tokens, kNumSyncTokens);
}

TEST_F(RasterImplementationGLESTest, WaitSyncTokenCHROMIUM) {
  GLbyte sync_token_data[GL_SYNC_TOKEN_SIZE_CHROMIUM] = {};

  EXPECT_CALL(*gl_, WaitSyncTokenCHROMIUM(sync_token_data)).Times(1);
  ri_->WaitSyncTokenCHROMIUM(sync_token_data);
}

TEST_F(RasterImplementationGLESTest, GetError) {
  const GLuint kGLInvalidOperation = GL_INVALID_OPERATION;

  EXPECT_CALL(*gl_, GetError()).WillOnce(Return(kGLInvalidOperation));
  GLenum error = ri_->GetError();
  EXPECT_EQ(kGLInvalidOperation, error);
}

TEST_F(RasterImplementationGLESTest, GetGraphicsResetStatusKHR) {
  const GLuint kGraphicsResetStatus = GL_UNKNOWN_CONTEXT_RESET_KHR;

  EXPECT_CALL(*gl_, GetGraphicsResetStatusKHR())
      .WillOnce(Return(kGraphicsResetStatus));
  GLenum status = ri_->GetGraphicsResetStatusKHR();
  EXPECT_EQ(kGraphicsResetStatus, status);
}

TEST_F(RasterImplementationGLESTest, LoseContextCHROMIUM) {
  const GLenum kCurrent = GL_GUILTY_CONTEXT_RESET_ARB;
  const GLenum kOther = GL_INNOCENT_CONTEXT_RESET_ARB;

  EXPECT_CALL(*gl_, LoseContextCHROMIUM(kCurrent, kOther)).Times(1);
  ri_->LoseContextCHROMIUM(kCurrent, kOther);
}

TEST_F(RasterImplementationGLESTest, GenQueriesEXT) {
  const GLsizei kNumQueries = 2;
  GLuint queries[kNumQueries] = {};

  EXPECT_CALL(*gl_, GenQueriesEXT(kNumQueries, queries)).Times(1);
  ri_->GenQueriesEXT(kNumQueries, queries);
}

TEST_F(RasterImplementationGLESTest, DeleteQueriesEXT) {
  const GLsizei kNumQueries = 2;
  GLuint queries[kNumQueries] = {2, 3};

  EXPECT_CALL(*gl_, DeleteQueriesEXT(kNumQueries, queries)).Times(1);
  ri_->DeleteQueriesEXT(kNumQueries, queries);
}

TEST_F(RasterImplementationGLESTest, BeginQueryEXT) {
  const GLenum kQueryTarget = GL_COMMANDS_ISSUED_CHROMIUM;
  const GLuint kQueryId = 23;

  EXPECT_CALL(*gl_, BeginQueryEXT(kQueryTarget, kQueryId)).Times(1);
  ri_->BeginQueryEXT(kQueryTarget, kQueryId);
}

TEST_F(RasterImplementationGLESTest, EndQueryEXT) {
  const GLenum kQueryTarget = GL_COMMANDS_ISSUED_CHROMIUM;

  EXPECT_CALL(*gl_, EndQueryEXT(kQueryTarget)).Times(1);
  ri_->EndQueryEXT(kQueryTarget);
}

TEST_F(RasterImplementationGLESTest, QueryCounterEXT) {
  const GLenum kQueryTarget = GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM;
  const GLuint kQueryId = 23;

  EXPECT_CALL(*gl_, QueryCounterEXT(kQueryId, kQueryTarget)).Times(1);
  ri_->QueryCounterEXT(kQueryId, kQueryTarget);
}

TEST_F(RasterImplementationGLESTest, GetQueryObjectuivEXT) {
  const GLuint kQueryId = 23;
  const GLsizei kQueryParam = GL_QUERY_RESULT_AVAILABLE_EXT;
  GLuint result = 0;

  EXPECT_CALL(*gl_, GetQueryObjectuivEXT(kQueryId, kQueryParam, &result))
      .Times(1);
  ri_->GetQueryObjectuivEXT(kQueryId, kQueryParam, &result);
}

TEST_F(RasterImplementationGLESTest, GetQueryObjectui64vEXT) {
  const GLuint kQueryId = 23;
  const GLsizei kQueryParam = GL_QUERY_RESULT_AVAILABLE_EXT;
  GLuint64 result = 0;

  EXPECT_CALL(*gl_, GetQueryObjectui64vEXT(kQueryId, kQueryParam, &result))
      .Times(1);
  ri_->GetQueryObjectui64vEXT(kQueryId, kQueryParam, &result);
}

TEST_F(RasterImplementationGLESTest, CreateAndConsumeForGpuRaster) {
  const GLuint kTextureId = 23;
  const auto mailbox = gpu::Mailbox::Generate();
  EXPECT_CALL(*gl_, CreateAndTexStorage2DSharedImageCHROMIUM(mailbox.name))
      .WillOnce(Return(kTextureId));
  GLuint texture_id = ri_->CreateAndConsumeForGpuRaster(mailbox);
  EXPECT_EQ(kTextureId, texture_id);
}

TEST_F(RasterImplementationGLESTest, DeleteGpuRasterTexture) {
  const GLuint kTextureId = 23;
  EXPECT_CALL(*gl_, DeleteTextures(1, Pointee(Eq(kTextureId)))).Times(1);
  ri_->DeleteGpuRasterTexture(kTextureId);
}

TEST_F(RasterImplementationGLESTest, BeginSharedImageAccess) {
  const GLuint kTextureId = 23;
  EXPECT_CALL(*gl_,
              BeginSharedImageAccessDirectCHROMIUM(
                  kTextureId, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM))
      .Times(1);
  ri_->BeginSharedImageAccessDirectCHROMIUM(
      kTextureId, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
}

TEST_F(RasterImplementationGLESTest, EndSharedImageAccess) {
  const GLuint kTextureId = 23;
  EXPECT_CALL(*gl_, EndSharedImageAccessDirectCHROMIUM(kTextureId)).Times(1);
  ri_->EndSharedImageAccessDirectCHROMIUM(kTextureId);
}

TEST_F(RasterImplementationGLESTest, BeginGpuRaster) {
  EXPECT_CALL(*gl_, TraceBeginCHROMIUM(StrEq("BeginGpuRaster"),
                                       StrEq("GpuRasterization")))
      .Times(1);
  ri_->BeginGpuRaster();
}

TEST_F(RasterImplementationGLESTest, EndGpuRaster) {
  EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_ALIGNMENT, 4)).Times(1);
  EXPECT_CALL(*gl_, TraceEndCHROMIUM()).Times(1);
  ri_->EndGpuRaster();
}

}  // namespace raster
}  // namespace gpu
