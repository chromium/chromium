// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/common/shared_image_info.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/raster_decoder_unittest_base.h"
#include "gpu/command_buffer/service/shared_image/test_image_backing.h"
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
  OOM(GL_GUILTY_CONTEXT_RESET, expected_reason_for_other_contexts);
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_EQ(error::kGuilty, GetContextLostReason());
}

TEST_P(RasterDecoderOOMTest, ContextLostReasonWhenStatusIsUnknown) {
  Init(/*has_robustness=*/true);
  // If there was a reset, it should override kOutOfMemory.
  const error::ContextLostReason expected_reason_for_other_contexts =
      error::kUnknown;
  OOM(GL_UNKNOWN_CONTEXT_RESET, expected_reason_for_other_contexts);
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

// The readback delivery gate must refuse to publish results after a device
// reset, and must stay silent otherwise. The shared image is registered with
// can_access=false so the readback command reaches the delivery gate without
// Skia touching the StrictMock GL interface; the gate runs after the copy
// helper regardless of the helper's outcome.
class RasterDecoderReadbackResetTest : public RasterDecoderManualInitTest {
 protected:
  // Shared client-visible tail for the healthy-path guards: the command
  // completes without denial -- no Result, no byte delivered from the
  // inaccessible backing, no context loss; the copy helper's INVALID_VALUE
  // is read so the fixture's no-unread-errors teardown check holds.
  void ExpectHealthyNonDelivery(const Mailbox& mailbox) {
    EXPECT_EQ(error::kNoError, IssueReadback(mailbox));
    EXPECT_EQ(0u, *Result());
    for (uint32_t i = 0; i < kPixelBytes; ++i) {
      EXPECT_EQ(0xAAu, Pixels()[i])
          << "pixel byte " << i << " unexpectedly modified";
    }
    EXPECT_FALSE(decoder_->WasContextLost());
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  }

  static constexpr GLuint kDstWidth = 2;
  static constexpr GLuint kDstHeight = 2;
  static constexpr GLuint kRowBytes = 8;
  static constexpr uint32_t kPixelBytes = kRowBytes * kDstHeight;
  // Shm layout at the shared memory base: Result, then pixel destination.
  static constexpr uint32_t kPixelsOffset = 16;

  void Init(bool with_workaround) {
    InitState init;
    init.extensions.push_back("GL_KHR_robustness");
    init.workarounds.check_graphics_reset_status_after_readback =
        with_workaround;
    InitDecoder(init);
  }

  Mailbox CreateInaccessibleSharedImage() {
    Mailbox mailbox = Mailbox::Generate();
    auto backing = std::make_unique<TestImageBacking>(
        mailbox,
        SharedImageInfo(viz::SinglePlaneFormat::kRGBA_8888, gfx::Size(4, 4),
                        gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
                        kPremul_SkAlphaType, SHARED_IMAGE_USAGE_RASTER_READ,
                        "TestLabel"),
        /*estimated_size=*/64);
    backing->SetCleared();
    backing->set_can_access(false);
    factory_ref_ =
        shared_image_manager()->Register(std::move(backing), &memory_tracker_);
    return mailbox;
  }

  error::Error IssueReadback(const Mailbox& mailbox) {
    auto& cmd =
        *GetImmediateAs<cmds::ReadbackARGBImagePixelsINTERNALImmediate>();
    // color_space_offset == pixels_offset encodes "no color space".
    cmd.Init(/*src_x=*/0, /*src_y=*/0, /*plane_index=*/0, kDstWidth, kDstHeight,
             kRowBytes, kRGBA_8888_SkColorType, kPremul_SkAlphaType,
             shared_memory_id_, shared_memory_offset_,
             /*color_space_offset=*/kPixelsOffset,
             /*pixels_offset=*/kPixelsOffset, mailbox.name);
    return ExecuteImmediateCmd(cmd, sizeof(mailbox.name));
  }

  uint32_t* Result() { return GetSharedMemoryAs<uint32_t*>(); }
  base::span<uint8_t> Pixels() {
    // SAFETY: the fixture's shared memory extends past `kPixelsOffset` by at
    // least `kPixelBytes`; every test addresses exactly that window.
    return UNSAFE_BUFFERS(
        base::span(GetSharedMemoryAs<uint8_t*>() + kPixelsOffset, kPixelBytes));
  }

  MemoryTypeTracker memory_tracker_{nullptr};
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref_;
};

// Tests that the raster decoder refuses ReadbackARGB delivery when the
// robustness status reports a reset latched during the readback -- the
// canvas-lane crossing of crbug.com/555394151 (ES 3.2 sec. 2.3.2). We
// expect kLostContext, Result left at zero, a scrubbed destination,
// INVALID_OPERATION, and a guilty loss via the SharedContextState observer
// path.
TEST_P(RasterDecoderReadbackResetTest,
       ReadbackARGBImagePixelsGuiltyResetDeniesResultDelivery) {
  Init(/*with_workaround=*/true);
  Mailbox mailbox = CreateInaccessibleSharedImage();
  *Result() = 0;
  std::ranges::fill(Pixels(), 0xAA);

  EXPECT_CALL(*gl_, GetError()).WillRepeatedly(Return(GL_NO_ERROR));
  EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
      .WillOnce(Return(GL_GUILTY_CONTEXT_RESET_KHR));

  EXPECT_EQ(error::kLostContext, IssueReadback(mailbox));
  EXPECT_EQ(0u, *Result());
  for (uint32_t i = 0; i < kPixelBytes; ++i) {
    EXPECT_EQ(0u, Pixels()[i]) << "pixel byte " << i << " not scrubbed";
  }
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_TRUE(decoder_->WasContextLostByRobustnessExtension());
  EXPECT_EQ(error::kGuilty, GetContextLostReason());
  // The gate reports the denial as INVALID_OPERATION; read it so the
  // fixture's no-unread-errors teardown check holds.
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

// Tests that healthy raster readbacks are delivered unchanged with the
// workaround enabled.
TEST_P(RasterDecoderReadbackResetTest,
       ReadbackARGBImagePixelsNoResetDoesNotDeny) {
  Init(/*with_workaround=*/true);
  Mailbox mailbox = CreateInaccessibleSharedImage();
  *Result() = 0;
  std::ranges::fill(Pixels(), 0xAA);

  EXPECT_CALL(*gl_, GetError()).WillRepeatedly(Return(GL_NO_ERROR));
  EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
      .Times(::testing::AtMost(1))
      .WillRepeatedly(Return(GL_NO_ERROR));

  // The inaccessible test backing makes the copy helper fail with a GL
  // error, but the gate must not fire.
  ExpectHealthyNonDelivery(mailbox);
}

// Same as ReadbackARGBImagePixelsNoResetDoesNotDeny, but with the
// workaround off: the readback path must issue no reset-status query at
// all.
TEST_P(RasterDecoderReadbackResetTest,
       ReadbackARGBImagePixelsWithoutWorkaroundDoesNotQueryResetStatus) {
  Init(/*with_workaround=*/false);
  Mailbox mailbox = CreateInaccessibleSharedImage();
  *Result() = 0;
  std::ranges::fill(Pixels(), 0xAA);

  EXPECT_CALL(*gl_, GetError()).WillRepeatedly(Return(GL_NO_ERROR));
  EXPECT_CALL(*gl_, GetGraphicsResetStatusARB()).Times(0);

  ExpectHealthyNonDelivery(mailbox);
}

// Note: The suite parameter is RasterDecoderTestBase's
// ignore_cached_state_for_test_.
INSTANTIATE_TEST_SUITE_P(Service,
                         RasterDecoderReadbackResetTest,
                         ::testing::Bool());

}  // namespace raster
}  // namespace gpu
