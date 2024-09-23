// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/raster_decoder_unittest_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArrayArgument;

namespace gpu {
namespace raster {

class RasterDecoderOOMTest : public RasterDecoderManualInitTest {
 protected:
  void Init(bool has_robustness) {
    InitState init;
    init.gl_version = "OpenGL ES 3.0";
    init.lose_context_when_out_of_memory = true;
    if (has_robustness) {
      init.extensions.push_back("GL_EXT_robustness");
    }
    InitDecoder(init);
  }

  void OOM(GLenum reset_status,
           error::ContextLostReason expected_other_reason) {
    if (context_->HasRobustness()) {
      EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
          .WillOnce(Return(reset_status));
      EXPECT_CALL(*gl_, GetError()).WillRepeatedly(Return(GL_CONTEXT_LOST_KHR));
    } else {
      EXPECT_CALL(*gl_, GetError()).WillRepeatedly(Return(GL_NO_ERROR));
    }

    // RasterDecoder::HandleGetError merges driver error state with decoder
    // error state.  Return GL_OUT_OF_MEMORY from decoder.
    GetDecoder()->SetOOMErrorForTest();

    cmds::GetError cmd;
    cmd.Init(shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kLostContext, ExecuteCmd(cmd));
    EXPECT_EQ(GL_OUT_OF_MEMORY,
              static_cast<GLint>(*GetSharedMemoryAs<GLenum*>()));
  }
};

// Test that we lose context.
TEST_P(RasterDecoderOOMTest, ContextLostReasonOOM) {
  Init(/*has_robustness=*/false);
  const error::ContextLostReason expected_reason_for_other_contexts =
      error::kOutOfMemory;
  OOM(GL_NO_ERROR, expected_reason_for_other_contexts);
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kOutOfMemory, GetContextLostReason());
}

TEST_P(RasterDecoderOOMTest, ContextLostReasonWhenStatusIsNoError) {
  Init(/*has_robustness=*/true);
  // If the reset status is NO_ERROR, we should be signaling kOutOfMemory.
  const error::ContextLostReason expected_reason_for_other_contexts =
      error::kOutOfMemory;
  OOM(GL_NO_ERROR, expected_reason_for_other_contexts);
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kOutOfMemory, GetContextLostReason());
}

TEST_P(RasterDecoderOOMTest, ContextLostReasonWhenStatusIsGuilty) {
  Init(/*has_robustness=*/true);
  // If there was a reset, it should override kOutOfMemory.
  const error::ContextLostReason expected_reason_for_other_contexts =
      error::kUnknown;
  OOM(GL_GUILTY_CONTEXT_RESET_ARB, expected_reason_for_other_contexts);
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kGuilty, GetContextLostReason());
}

TEST_P(RasterDecoderOOMTest, ContextLostReasonWhenStatusIsUnknown) {
  Init(/*has_robustness=*/true);
  // If there was a reset, it should override kOutOfMemory.
  const error::ContextLostReason expected_reason_for_other_contexts =
      error::kUnknown;
  OOM(GL_UNKNOWN_CONTEXT_RESET_ARB, expected_reason_for_other_contexts);
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kUnknown, GetContextLostReason());
}

INSTANTIATE_TEST_SUITE_P(Service, RasterDecoderOOMTest, ::testing::Bool());

class RasterDecoderLostContextTest : public RasterDecoderManualInitTest {
 protected:
  void Init(bool has_robustness) {
    InitState init;
    if (has_robustness) {
      init.extensions.push_back("GL_KHR_robustness");
    }
    InitDecoder(init);
  }

  void InitWithVirtualContextsAndRobustness() {
    InitState init;
    init.extensions.push_back("GL_KHR_robustness");
    init.workarounds.use_virtualized_gl_contexts = true;
    InitDecoder(init);
  }

  void DoGetErrorWithContextLost(GLenum reset_status) {
    DCHECK(context_->HasExtension("GL_KHR_robustness"));
    // Once context loss has occurred, driver will always return
    // GL_CONTEXT_LOST_KHR.
    EXPECT_CALL(*gl_, GetError()).WillRepeatedly(Return(GL_CONTEXT_LOST_KHR));
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

TEST_P(RasterDecoderLostContextTest, LostFromMakeCurrent) {
  Init(/*has_robustness=*/false);
  EXPECT_CALL(*context_, MakeCurrentImpl(surface_.get()))
      .WillOnce(Return(false));
  EXPECT_FALSE(decoder_->WasContextLost());
  decoder_->MakeCurrent();
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kMakeCurrentFailed, GetContextLostReason());

  // We didn't process commands, so we need to clear the decoder error,
  // so that we can shut down cleanly.
  ClearCurrentDecoderError();
}

TEST_P(RasterDecoderLostContextTest, LostFromDriverOOM) {
  Init(/*has_robustness=*/false);
  EXPECT_CALL(*context_, MakeCurrentImpl(surface_.get()))
      .WillOnce(Return(true));
  EXPECT_CALL(*gl_, GetError()).WillOnce(Return(GL_OUT_OF_MEMORY));
  EXPECT_FALSE(decoder_->WasContextLost());
  decoder_->MakeCurrent();
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kOutOfMemory, GetContextLostReason());

  // We didn't process commands, so we need to clear the decoder error,
  // so that we can shut down cleanly.
  ClearCurrentDecoderError();
}

TEST_P(RasterDecoderLostContextTest, LostFromMakeCurrentWithRobustness) {
  Init(/*has_robustness=*/true);  // with robustness
  // If we can't make the context current, we cannot query the robustness
  // extension.
  EXPECT_CALL(*gl_, GetGraphicsResetStatusARB()).Times(0);
  EXPECT_CALL(*context_, MakeCurrentImpl(surface_.get()))
      .WillOnce(Return(false));
  decoder_->MakeCurrent();
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_FALSE(decoder_->WasContextLostByRobustnessExtension());
  EXPECT_EQ(error::kMakeCurrentFailed, GetContextLostReason());

  // We didn't process commands, so we need to clear the decoder error,
  // so that we can shut down cleanly.
  ClearCurrentDecoderError();
}

TEST_P(RasterDecoderLostContextTest, QueryDestroyAfterLostFromMakeCurrent) {
  Init(/*has_robustness=*/false);

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

  decoder_->MakeCurrent();
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kMakeCurrentFailed, GetContextLostReason());
  ClearCurrentDecoderError();
  ResetDecoder();
}

TEST_P(RasterDecoderLostContextTest, LostFromResetAfterMakeCurrent) {
  Init(/*has_robustness=*/true);
  InSequence seq;
  EXPECT_CALL(*context_, MakeCurrentImpl(surface_.get()))
      .WillOnce(Return(true));
  EXPECT_CALL(*gl_, GetError()).WillOnce(Return(GL_CONTEXT_LOST_KHR));
  EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
      .WillOnce(Return(GL_GUILTY_CONTEXT_RESET_KHR));
  decoder_->MakeCurrent();
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_TRUE(decoder_->WasContextLostByRobustnessExtension());
  EXPECT_EQ(error::kGuilty, GetContextLostReason());

  // We didn't process commands, so we need to clear the decoder error,
  // so that we can shut down cleanly.
  ClearCurrentDecoderError();
}

TEST_P(RasterDecoderLostContextTest, LoseGuiltyFromGLError) {
  Init(/*has_robustness=*/true);
  DoGetErrorWithContextLost(GL_GUILTY_CONTEXT_RESET_KHR);
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_TRUE(decoder_->WasContextLostByRobustnessExtension());
  EXPECT_EQ(error::kGuilty, GetContextLostReason());
}

TEST_P(RasterDecoderLostContextTest, LoseInnocentFromGLError) {
  Init(/*has_robustness=*/true);
  DoGetErrorWithContextLost(GL_INNOCENT_CONTEXT_RESET_KHR);
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_TRUE(decoder_->WasContextLostByRobustnessExtension());
  EXPECT_EQ(error::kInnocent, GetContextLostReason());
}

INSTANTIATE_TEST_SUITE_P(Service,
                         RasterDecoderLostContextTest,
                         ::testing::Bool());

}  // namespace raster
}  // namespace gpu
