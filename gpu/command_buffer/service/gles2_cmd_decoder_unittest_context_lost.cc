// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"

#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/gl_surface_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace gpu {
namespace gles2 {

class GLES2DecoderDrawOOMTest : public GLES2DecoderManualInitTest {
 protected:
  void Init(bool has_robustness) {
    InitState init;
    init.lose_context_when_out_of_memory = true;
    if (has_robustness)
      init.extensions = "GL_ARB_robustness";
    InitDecoder(init);
    SetupDefaultProgram();
  }

  void Draw(GLenum reset_status,
            error::ContextLostReason expected_other_reason) {
    const GLsizei kFakeLargeCount = 0x1234;
    SetupTexture();
    if (context_->HasRobustness()) {
      EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
          .WillOnce(Return(reset_status));
    }
    AddExpectationsForSimulatedAttrib0WithError(kFakeLargeCount, 0,
                                                GL_OUT_OF_MEMORY);
    EXPECT_CALL(*gl_, DrawArrays(_, _, _)).Times(0).RetiresOnSaturation();
    // Other contexts in the group should be lost also.
    EXPECT_CALL(*mock_decoder_, MarkContextLost(expected_other_reason))
        .Times(1)
        .RetiresOnSaturation();
    cmds::DrawArrays cmd;
    cmd.Init(GL_TRIANGLES, 0, kFakeLargeCount);
    EXPECT_EQ(error::kLostContext, ExecuteCmd(cmd));
  }
};

// Test that we lose context.
TEST_P(GLES2DecoderDrawOOMTest, ContextLostReasonOOM) {
  Init(false);  // without robustness
  const error::ContextLostReason expected_reason_for_other_contexts =
      error::kOutOfMemory;
  Draw(GL_NO_ERROR, expected_reason_for_other_contexts);
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kOutOfMemory, GetContextLostReason());
}

TEST_P(GLES2DecoderDrawOOMTest, ContextLostReasonWhenStatusIsNoError) {
  Init(true);  // with robustness
  // If the reset status is NO_ERROR, we should be signaling kOutOfMemory.
  const error::ContextLostReason expected_reason_for_other_contexts =
      error::kOutOfMemory;
  Draw(GL_NO_ERROR, expected_reason_for_other_contexts);
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kOutOfMemory, GetContextLostReason());
}

TEST_P(GLES2DecoderDrawOOMTest, ContextLostReasonWhenStatusIsGuilty) {
  Init(true);
  // If there was a reset, it should override kOutOfMemory.
  const error::ContextLostReason expected_reason_for_other_contexts =
      error::kUnknown;
  Draw(GL_GUILTY_CONTEXT_RESET_ARB, expected_reason_for_other_contexts);
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kGuilty, GetContextLostReason());
}

TEST_P(GLES2DecoderDrawOOMTest, ContextLostReasonWhenStatusIsUnknown) {
  Init(true);
  // If there was a reset, it should override kOutOfMemory.
  const error::ContextLostReason expected_reason_for_other_contexts =
      error::kUnknown;
  Draw(GL_UNKNOWN_CONTEXT_RESET_ARB, expected_reason_for_other_contexts);
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kUnknown, GetContextLostReason());
}

INSTANTIATE_TEST_SUITE_P(Service, GLES2DecoderDrawOOMTest, ::testing::Bool());

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
  EXPECT_CALL(*context_, MakeCurrent(surface_.get())).WillOnce(Return(false));
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
  EXPECT_CALL(*context_, MakeCurrent(surface_.get())).WillOnce(Return(false));
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
  EXPECT_CALL(*context_, MakeCurrent(surface_.get())).WillOnce(Return(false));
  // Expect the group to be lost.
  EXPECT_CALL(*mock_decoder_, MarkContextLost(error::kUnknown)).Times(1);

  decoder_->MakeCurrent();
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kMakeCurrentFailed, GetContextLostReason());
  ClearCurrentDecoderError();
}

TEST_P(GLES2DecoderLostContextTest, QueryDestroyAfterLostFromMakeCurrent) {
  InitState init;
  init.extensions = "GL_EXT_occlusion_query_boolean GL_ARB_sync";
  init.gl_version = "2.0";
  init.has_alpha = true;
  init.request_alpha = true;
  init.bind_generates_resource = true;
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
  EXPECT_CALL(*context_, MakeCurrent(surface_.get())).WillOnce(Return(false));
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
  EXPECT_CALL(*context_, MakeCurrent(surface_.get())).WillOnce(Return(true));
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

}  // namespace gles2
}  // namespace gpu
