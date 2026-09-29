// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/gl_surface_mock.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace gpu {
namespace gles2 {

class GLES2DecoderLostContextTest : public GLES2DecoderManualInitTest {
 protected:
  void Init(bool has_robustness) {
    InitState init;
    init.gl_version = "OpenGL ES 2.0";
    if (has_robustness)
      init.extensions = "GL_KHR_robustness";
    InitDecoder(init);
  }

  void DoGetErrorWithContextLost(GLenum reset_status) {
    DCHECK(context_->HasExtension("GL_KHR_robustness"));
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_CONTEXT_LOST_KHR))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
        .WillOnce(Return(reset_status));
    cmds::GetError cmd;
    cmd.Init(shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kLostContext, ExecuteCmd(cmd));
    EXPECT_EQ(static_cast<GLuint>(GL_NO_ERROR), *GetSharedMemoryAs<GLenum*>());
  }

  void ClearCurrentDecoderError() {
    DCHECK(decoder_->WasContextLost());
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_CONTEXT_LOST_KHR))
        .RetiresOnSaturation();
    cmds::GetError cmd;
    cmd.Init(shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kLostContext, ExecuteCmd(cmd));
  }
};

TEST_P(GLES2DecoderLostContextTest, LostFromMakeCurrent) {
  Init(false);  // without robustness
  EXPECT_CALL(*context_, MakeCurrentImpl(surface_.get()))
      .WillOnce(Return(false));
  // Expect the group to be lost.
  EXPECT_CALL(*mock_decoder_, MarkContextLost(error::kUnknown)).Times(1);
  decoder_->MakeCurrent();
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kMakeCurrentFailed, GetContextLostReason());

  // We didn't process commands, so we need to clear the decoder error,
  // so that we can shut down cleanly.
  ClearCurrentDecoderError();
}

TEST_P(GLES2DecoderLostContextTest, LostFromMakeCurrentWithRobustness) {
  Init(true);  // with robustness
  // If we can't make the context current, we cannot query the robustness
  // extension.
  EXPECT_CALL(*gl_, GetGraphicsResetStatusARB()).Times(0);
  EXPECT_CALL(*context_, MakeCurrentImpl(surface_.get()))
      .WillOnce(Return(false));
  // Expect the group to be lost.
  EXPECT_CALL(*mock_decoder_, MarkContextLost(error::kUnknown)).Times(1);
  decoder_->MakeCurrent();
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_FALSE(decoder_->WasContextLostByRobustnessExtension());
  EXPECT_EQ(error::kMakeCurrentFailed, GetContextLostReason());

  // We didn't process commands, so we need to clear the decoder error,
  // so that we can shut down cleanly.
  ClearCurrentDecoderError();
}

TEST_P(GLES2DecoderLostContextTest, TextureDestroyAfterLostFromMakeCurrent) {
  Init(true);
  // Create a texture and framebuffer, and attach the texture to the
  // framebuffer.
  const GLuint kClientTextureId = 4100;
  const GLuint kServiceTextureId = 4101;
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kClientTextureId);
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 5, 6, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               shared_memory_id_, kSharedMemoryOffset);
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  DoFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         kClientTextureId, kServiceTextureId, 0, GL_NO_ERROR);

  // The texture should never be deleted at the GL level.
  EXPECT_CALL(*gl_, DeleteTextures(1, Pointee(kServiceTextureId)))
      .Times(0)
      .RetiresOnSaturation();

  DoBindFramebuffer(GL_FRAMEBUFFER, 0, 0);
  EXPECT_CALL(*gl_, BindTexture(_, 0)).Times(testing::AnyNumber());
  GenHelper<cmds::DeleteTexturesImmediate>(kClientTextureId);

  // Force context lost for MakeCurrent().
  EXPECT_CALL(*context_, MakeCurrentImpl(surface_.get()))
      .WillOnce(Return(false));
  // Expect the group to be lost.
  EXPECT_CALL(*mock_decoder_, MarkContextLost(error::kUnknown)).Times(1);

  decoder_->MakeCurrent();
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kMakeCurrentFailed, GetContextLostReason());
  ClearCurrentDecoderError();
}

TEST_P(GLES2DecoderLostContextTest, QueryDestroyAfterLostFromMakeCurrent) {
  InitState init;
  init.extensions = "GL_EXT_occlusion_query_boolean";
  init.gl_version = "OpenGL ES 3.0";
  init.has_alpha = true;
  init.request_alpha = true;
  InitDecoder(init);

  const GLsync kGlSync = reinterpret_cast<GLsync>(0xdeadbeef);
  GenHelper<cmds::GenQueriesEXTImmediate>(kNewClientId);

  cmds::BeginQueryEXT begin_cmd;
  begin_cmd.Init(GL_COMMANDS_COMPLETED_CHROMIUM, kNewClientId,
                 shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != nullptr);
  QueryManager::Query* query = query_manager->GetQuery(kNewClientId);
  ASSERT_TRUE(query != nullptr);
  EXPECT_FALSE(query->IsPending());

  EXPECT_CALL(*gl_, Flush()).RetiresOnSaturation();
  EXPECT_CALL(*gl_, FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0))
      .WillOnce(Return(kGlSync))
      .RetiresOnSaturation();
#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();
#endif

  cmds::EndQueryEXT end_cmd;
  end_cmd.Init(GL_COMMANDS_COMPLETED_CHROMIUM, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync)).Times(0).RetiresOnSaturation();
#endif
  EXPECT_CALL(*gl_, DeleteSync(kGlSync)).Times(0).RetiresOnSaturation();

  // Force context lost for MakeCurrent().
  EXPECT_CALL(*context_, MakeCurrentImpl(surface_.get()))
      .WillOnce(Return(false));
  // Expect the group to be lost.
  EXPECT_CALL(*mock_decoder_, MarkContextLost(error::kUnknown)).Times(1);

  decoder_->MakeCurrent();
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kMakeCurrentFailed, GetContextLostReason());
  ClearCurrentDecoderError();
  ResetDecoder();
}

TEST_P(GLES2DecoderLostContextTest, LostFromResetAfterMakeCurrent) {
  Init(true);  // with robustness
  InSequence seq;
  EXPECT_CALL(*context_, MakeCurrentImpl(surface_.get()))
      .WillOnce(Return(true));
  EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
      .WillOnce(Return(GL_GUILTY_CONTEXT_RESET_KHR));
  // Expect the group to be lost.
  EXPECT_CALL(*mock_decoder_, MarkContextLost(error::kUnknown)).Times(1);
  decoder_->MakeCurrent();
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_TRUE(decoder_->WasContextLostByRobustnessExtension());
  EXPECT_EQ(error::kGuilty, GetContextLostReason());

  // We didn't process commands, so we need to clear the decoder error,
  // so that we can shut down cleanly.
  ClearCurrentDecoderError();
}

TEST_P(GLES2DecoderLostContextTest, LoseGuiltyFromGLError) {
  Init(true);
  // Always expect other contexts to be signaled as 'kUnknown' since we can't
  // query their status without making them current.
  EXPECT_CALL(*mock_decoder_, MarkContextLost(error::kUnknown))
      .Times(1);
  DoGetErrorWithContextLost(GL_GUILTY_CONTEXT_RESET_KHR);
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_TRUE(decoder_->WasContextLostByRobustnessExtension());
  EXPECT_EQ(error::kGuilty, GetContextLostReason());
}

TEST_P(GLES2DecoderLostContextTest, LoseInnocentFromGLError) {
  Init(true);
  // Always expect other contexts to be signaled as 'kUnknown' since we can't
  // query their status without making them current.
  EXPECT_CALL(*mock_decoder_, MarkContextLost(error::kUnknown))
      .Times(1);
  DoGetErrorWithContextLost(GL_INNOCENT_CONTEXT_RESET_KHR);
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_TRUE(decoder_->WasContextLostByRobustnessExtension());
  EXPECT_EQ(error::kInnocent, GetContextLostReason());
}

TEST_P(GLES2DecoderLostContextTest, LoseGroupFromRobustness) {
  // If one context in a group is lost through robustness,
  // the other ones should also get lost and query the reset status.
  Init(true);
  EXPECT_CALL(*mock_decoder_, MarkContextLost(error::kUnknown))
      .Times(1);
  // There should be no GL calls, since we might not have a current context.
  EXPECT_CALL(*gl_, GetGraphicsResetStatusARB()).Times(0);
  LoseContexts(error::kUnknown);
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kUnknown, GetContextLostReason());

  // We didn't process commands, so we need to clear the decoder error,
  // so that we can shut down cleanly.
  ClearCurrentDecoderError();
}

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderLostContextTest,
                         ::testing::Bool());

class GLES2DecoderReadbackResetTest : public GLES2DecoderManualInitTest {
 protected:
  void Init(bool with_workaround) {
    gpu::GpuDriverBugWorkarounds workarounds;
    workarounds.check_graphics_reset_status_after_readback = with_workaround;
    InitState init;
    // Note: ES3 init is REQUIRED: HandleGetBufferSubDataCHROMIUM dereferences
    // the default transform feedback object, which only exists on an ES3
    // context.
    init.gl_version = "OpenGL ES 3.0";
    init.context_type = CONTEXT_TYPE_OPENGLES3;
    init.extensions = "GL_KHR_robustness";
    InitDecoderWithWorkarounds(init, workarounds);
  }

  void ClearCurrentDecoderError() {
    DCHECK(decoder_->WasContextLost());
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_CONTEXT_LOST_KHR))
        .RetiresOnSaturation();
    cmds::GetError cmd;
    cmd.Init(shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kLostContext, ExecuteCmd(cmd));
  }
};

// Tests that GetBufferSubData refuses delivery when the robustness poll
// after the blocking map reports a reset (crbug.com/558109451). The mocked
// driver returns a garbage-filled mapping, then GL_GUILTY_CONTEXT_RESET.
// Per ES 3.2 sec. 2.3.2 the result must not be delivered. We expect
// kLostContext, an unmap with no copy, the destination sentinel intact,
// and a guilty context loss.
TEST_P(GLES2DecoderReadbackResetTest, GetBufferSubDataRefusedAfterReset) {
  Init(/*with_workaround=*/true);
  const GLsizeiptr kSize = 64;
  DoBindBuffer(GL_PIXEL_PACK_BUFFER, client_buffer_id_, kServiceBufferId);
  DoBufferData(GL_PIXEL_PACK_BUFFER, kSize);

  // SAFETY: the fixture's shared memory is at least kSize bytes.
  base::span<uint8_t> dest = UNSAFE_BUFFERS(
      base::span(GetSharedMemoryAs<uint8_t*>(), static_cast<size_t>(kSize)));
  std::ranges::fill(dest, 0xAB);
  static std::array<uint8_t, kSize> garbage;
  garbage.fill(0xD8);

  // Losing this context broadcasts to the rest of the group.
  EXPECT_CALL(*mock_decoder_, MarkContextLost(error::kUnknown)).Times(1);
  {
    InSequence seq;
    EXPECT_CALL(*gl_,
                MapBufferRange(GL_PIXEL_PACK_BUFFER, 0, kSize, GL_MAP_READ_BIT))
        .WillOnce(Return(garbage.data()))
        .RetiresOnSaturation();
    // The gate polls the reset status after the map returns; the driver
    // reports the latched queue-group fatal.
    EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
        .WillOnce(Return(GL_GUILTY_CONTEXT_RESET_KHR))
        .RetiresOnSaturation();
  }

  cmds::GetBufferSubDataCHROMIUM cmd;
  cmd.Init(GL_PIXEL_PACK_BUFFER, 0, kSize, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kLostContext, ExecuteCmd(cmd));

  // Delivery must be refused: the sentinel survives, the garbage does not
  // reach renderer-visible memory, and the context is lost as guilty.
  for (GLsizeiptr i = 0; i < kSize; ++i) {
    EXPECT_EQ(0xABu, dest[i]) << "byte " << i << " delivered";
  }
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_TRUE(decoder_->WasContextLostByRobustnessExtension());
  EXPECT_EQ(error::kGuilty, GetContextLostReason());

  // Consume the latched context-loss decoder error so the fixture can shut
  // down cleanly (same pattern as GLES2DecoderLostContextTest).
  ClearCurrentDecoderError();
}

// Same shape as GetBufferSubDataRefusedAfterReset, but the poll reports
// NO_ERROR: the mapped bytes must be delivered unchanged. This ensures the
// workaround does not regress healthy GetBufferSubData.
TEST_P(GLES2DecoderReadbackResetTest, GetBufferSubDataDeliveredWhenNoReset) {
  Init(/*with_workaround=*/true);
  const GLsizeiptr kSize = 64;
  DoBindBuffer(GL_PIXEL_PACK_BUFFER, client_buffer_id_, kServiceBufferId);
  DoBufferData(GL_PIXEL_PACK_BUFFER, kSize);

  // SAFETY: the fixture's shared memory is at least kSize bytes.
  base::span<uint8_t> dest = UNSAFE_BUFFERS(
      base::span(GetSharedMemoryAs<uint8_t*>(), static_cast<size_t>(kSize)));
  std::ranges::fill(dest, 0xAB);
  static std::array<uint8_t, kSize> garbage;
  garbage.fill(0xD8);

  EXPECT_CALL(*gl_,
              MapBufferRange(GL_PIXEL_PACK_BUFFER, 0, kSize, GL_MAP_READ_BIT))
      .WillOnce(Return(garbage.data()))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
      .Times(AnyNumber())
      .WillRepeatedly(Return(GL_NO_ERROR));
  EXPECT_CALL(*gl_, UnmapBuffer(GL_PIXEL_PACK_BUFFER))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();

  cmds::GetBufferSubDataCHROMIUM cmd;
  cmd.Init(GL_PIXEL_PACK_BUFFER, 0, kSize, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  for (GLsizeiptr i = 0; i < kSize; ++i) {
    EXPECT_EQ(0xD8u, dest[i]) << "byte " << i << " not delivered";
  }
  EXPECT_FALSE(decoder_->WasContextLost());
}

// Same as GetBufferSubDataDeliveredWhenNoReset, but with the workaround
// off and the reset-status query expected exactly zero times.
TEST_P(GLES2DecoderReadbackResetTest,
       GetBufferSubDataNoResetCheckWithoutWorkaround) {
  Init(/*with_workaround=*/false);
  const GLsizeiptr kSize = 64;
  DoBindBuffer(GL_PIXEL_PACK_BUFFER, client_buffer_id_, kServiceBufferId);
  DoBufferData(GL_PIXEL_PACK_BUFFER, kSize);

  // SAFETY: the fixture's shared memory is at least kSize bytes.
  base::span<uint8_t> dest = UNSAFE_BUFFERS(
      base::span(GetSharedMemoryAs<uint8_t*>(), static_cast<size_t>(kSize)));
  std::ranges::fill(dest, 0xAB);
  static std::array<uint8_t, kSize> garbage;
  garbage.fill(0xD8);

  EXPECT_CALL(*gl_, GetGraphicsResetStatusARB()).Times(0);
  EXPECT_CALL(*gl_,
              MapBufferRange(GL_PIXEL_PACK_BUFFER, 0, kSize, GL_MAP_READ_BIT))
      .WillOnce(Return(garbage.data()))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, UnmapBuffer(GL_PIXEL_PACK_BUFFER))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();

  cmds::GetBufferSubDataCHROMIUM cmd;
  cmd.Init(GL_PIXEL_PACK_BUFFER, 0, kSize, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  for (GLsizeiptr i = 0; i < kSize; ++i) {
    EXPECT_EQ(0xD8u, dest[i]) << "byte " << i << " not delivered";
  }
  EXPECT_FALSE(decoder_->WasContextLost());
}

// Tests that the buffer shadow-copy refresh refuses to ingest mapped bytes
// after a reset -- the second buffer-map delivery point of
// crbug.com/558109451. The shadow shared memory keeps its prior contents
// and the context is lost.
TEST_P(GLES2DecoderReadbackResetTest, ShadowCopyRefusedAfterReset) {
  Init(/*with_workaround=*/true);
  const GLsync kGlSync = reinterpret_cast<GLsync>(0xdeadbeef);
  const GLuint kSize = 64;
  const uint32_t kShadowOffset = kSharedMemoryOffset + 128;

  // The readback callback binds the scratch binding and (on the refusal
  // path) never restores it; specific expectations declared later win over
  // this blanket.
  EXPECT_CALL(*gl_, BindBuffer(_, _)).Times(AnyNumber());

  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  DoBufferData(GL_ARRAY_BUFFER, kSize);

  cmds::SetReadbackBufferShadowAllocationINTERNAL shadow_cmd;
  shadow_cmd.Init(client_buffer_id_, shared_memory_id_, kShadowOffset, kSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(shadow_cmd));

  // SAFETY: shared memory extends past kShadowOffset by at least kSize.
  base::span<uint8_t> shadow = UNSAFE_BUFFERS(base::span(
      GetSharedMemoryAs<uint8_t*>() + (kShadowOffset - kSharedMemoryOffset),
      static_cast<size_t>(kSize)));
  std::ranges::fill(shadow, 0xAB);
  static std::array<uint8_t, kSize> garbage;
  garbage.fill(0xD8);

  GenHelper<cmds::GenQueriesEXTImmediate>(kNewClientId);
  cmds::BeginQueryEXT begin_cmd;
  begin_cmd.Init(GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM, kNewClientId,
                 shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));

  // HandleEndQueryEXT adds the readback callback while the query is still
  // active (not yet pending), so QueryManager::Query::AddCallback runs it
  // synchronously inside the EndQuery command: map, poll (guilty), refuse
  // the copy. The fence for the now-ending query is still created after the
  // refusal (the handler continues), and the command surfaces the context loss.
  EXPECT_CALL(*mock_decoder_, MarkContextLost(error::kUnknown)).Times(1);
  {
    InSequence seq;
    EXPECT_CALL(*gl_,
                MapBufferRange(GL_ARRAY_BUFFER, 0, kSize, GL_MAP_READ_BIT))
        .WillOnce(Return(garbage.data()))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
        .WillOnce(Return(GL_GUILTY_CONTEXT_RESET_KHR))
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, Flush()).Times(AnyNumber());
  EXPECT_CALL(*gl_, FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0))
      .Times(AnyNumber())
      .WillRepeatedly(Return(kGlSync));
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .Times(AnyNumber())
      .WillRepeatedly(Return(GL_TRUE));

  cmds::EndQueryEXT end_cmd;
  end_cmd.Init(GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM, 1);
  EXPECT_EQ(error::kLostContext, ExecuteCmd(end_cmd));

  // Delivery must be refused: the shadow shm keeps its sentinel and the
  // context is lost as guilty via the robustness extension.
  for (GLuint i = 0; i < kSize; ++i) {
    EXPECT_EQ(0xABu, shadow[i]) << "byte " << i << " delivered";
  }
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_TRUE(decoder_->WasContextLostByRobustnessExtension());
  EXPECT_EQ(error::kGuilty, GetContextLostReason());

  // The fence must not be touched after the loss (no live-context teardown).
  EXPECT_CALL(*gl_, DeleteSync(kGlSync)).Times(0).RetiresOnSaturation();

  // No ClearCurrentDecoderError() here: the EndQuery command itself already
  // surfaced (and consumed) the latched decoder error -- its handler returns
  // kNoError, so DoCommandsImpl swaps in current_decoder_error_ and clears
  // it, unlike the GetBufferSubData tests whose handler returns kLostContext
  // directly.
  ResetDecoder();
}

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderReadbackResetTest,
                         ::testing::Bool());

}  // namespace gles2
}  // namespace gpu
