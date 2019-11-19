// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/raster_decoder.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/raster_decoder_unittest_base.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_image_stub.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace gpu {
namespace raster {

namespace {

void CopyMailboxes(GLbyte (&output)[sizeof(Mailbox) * 2],
                   const Mailbox& source,
                   const Mailbox& dest) {
  memcpy(output, source.name, sizeof(source.name));
  memcpy(output + sizeof(source.name), dest.name, sizeof(dest.name));
}

}  // anonymous namespace

class RasterDecoderTest : public RasterDecoderTestBase {
 public:
  RasterDecoderTest() = default;
};

INSTANTIATE_TEST_SUITE_P(Service, RasterDecoderTest, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(Service,
                         RasterDecoderManualInitTest,
                         ::testing::Bool());

const GLsync kGlSync = reinterpret_cast<GLsync>(0xdeadbeef);

TEST_P(RasterDecoderTest, BeginEndQueryEXTCommandsCompletedCHROMIUM) {
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
  EXPECT_TRUE(query->IsActive());

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
  EXPECT_TRUE(query->IsPending());
  EXPECT_FALSE(query->IsActive());

#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();
#endif
  EXPECT_CALL(*gl_, ClientWaitSync(kGlSync, _, _))
      .WillOnce(Return(GL_TIMEOUT_EXPIRED))
      .RetiresOnSaturation();
  query_manager->ProcessPendingQueries(false);

  EXPECT_TRUE(query->IsPending());

#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();
#endif
  EXPECT_CALL(*gl_, ClientWaitSync(kGlSync, _, _))
      .WillOnce(Return(GL_ALREADY_SIGNALED))
      .RetiresOnSaturation();
  query_manager->ProcessPendingQueries(false);

  EXPECT_FALSE(query->IsPending());

#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();
#endif
  EXPECT_CALL(*gl_, DeleteSync(kGlSync)).Times(1).RetiresOnSaturation();
  ResetDecoder();
}

TEST_P(RasterDecoderTest, BeginEndQueryEXTCommandsIssuedCHROMIUM) {
  cmds::BeginQueryEXT begin_cmd;

  GenHelper<cmds::GenQueriesEXTImmediate>(kNewClientId);

  // Test valid parameters work.
  begin_cmd.Init(GL_COMMANDS_ISSUED_CHROMIUM, kNewClientId, shared_memory_id_,
                 kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != nullptr);
  QueryManager::Query* query = query_manager->GetQuery(kNewClientId);
  ASSERT_TRUE(query != nullptr);
  EXPECT_FALSE(query->IsPending());
  EXPECT_TRUE(query->IsActive());

  // Test end succeeds.
  cmds::EndQueryEXT end_cmd;
  end_cmd.Init(GL_COMMANDS_ISSUED_CHROMIUM, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_FALSE(query->IsPending());
  EXPECT_FALSE(query->IsActive());
}

TEST_P(RasterDecoderTest, QueryCounterEXTCommandsIssuedTimestampCHROMIUM) {
  GenHelper<cmds::GenQueriesEXTImmediate>(kNewClientId);

  cmds::QueryCounterEXT query_counter_cmd;
  query_counter_cmd.Init(kNewClientId, GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM,
                         shared_memory_id_, kSharedMemoryOffset, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(query_counter_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != nullptr);
  QueryManager::Query* query = query_manager->GetQuery(kNewClientId);
  ASSERT_TRUE(query != nullptr);
  EXPECT_FALSE(query->IsPending());
  EXPECT_FALSE(query->IsActive());
}

TEST_P(RasterDecoderTest, CopyTexSubImage2DSizeMismatch) {
  shared_context_state_->set_need_context_state_reset(true);
  // Create uninitialized source texture.
  gpu::Mailbox source_texture_mailbox =
      CreateFakeTexture(kNewServiceId, viz::ResourceFormat::RGBA_8888,
                        /*width=*/1, /*height=*/1,
                        /*cleared=*/true);
  GLbyte mailboxes[sizeof(gpu::Mailbox) * 2];
  CopyMailboxes(mailboxes, source_texture_mailbox, client_texture_mailbox_);

  SharedImageRepresentationFactory repr_factory(shared_image_manager(),
                                                nullptr);
  auto representation = repr_factory.ProduceGLTexture(client_texture_mailbox_);
  gles2::Texture* dest_texture = representation->GetTexture();

  {
    // This will initialize the bottom right corner of destination.
    SetScopedTextureBinderExpectations(GL_TEXTURE_2D);
    auto& cmd = *GetImmediateAs<cmds::CopySubTextureINTERNALImmediate>();
    cmd.Init(1, 1, 0, 0, 1, 1, mailboxes);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailboxes)));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_EQ(dest_texture->GetLevelClearedRect(GL_TEXTURE_2D, 0),
              gfx::Rect(1, 1, 1, 1));
  }

  {
    // Dest rect outside of dest bounds
    auto& cmd = *GetImmediateAs<cmds::CopySubTextureINTERNALImmediate>();
    cmd.Init(2, 2, 0, 0, 1, 1, mailboxes);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailboxes)));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
    EXPECT_EQ(dest_texture->GetLevelClearedRect(GL_TEXTURE_2D, 0),
              gfx::Rect(1, 1, 1, 1));
  }

  {
    // Source rect outside of source bounds
    auto& cmd = *GetImmediateAs<cmds::CopySubTextureINTERNALImmediate>();
    cmd.Init(0, 0, 0, 0, 2, 2, mailboxes);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailboxes)));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
    EXPECT_EQ(dest_texture->GetLevelClearedRect(GL_TEXTURE_2D, 0),
              gfx::Rect(1, 1, 1, 1));
  }
}

TEST_P(RasterDecoderTest, CopyTexSubImage2DTwiceClearsUnclearedTexture) {
  shared_context_state_->set_need_context_state_reset(true);
  // Create uninitialized source texture.
  gpu::Mailbox source_texture_mailbox =
      CreateFakeTexture(kNewServiceId, viz::ResourceFormat::RGBA_8888,
                        /*width=*/2, /*height=*/2,
                        /*cleared=*/false);
  GLbyte mailboxes[sizeof(gpu::Mailbox) * 2];
  CopyMailboxes(mailboxes, source_texture_mailbox, client_texture_mailbox_);

  // This will initialize the top half of destination.
  {
    // Source is undefined, so first call to CopySubTexture will clear the
    // source.
    SetupClearTextureExpectations(kNewServiceId, kServiceTextureId,
                                  GL_TEXTURE_2D, GL_TEXTURE_2D, 0, GL_RGBA,
                                  GL_UNSIGNED_BYTE, 0, 0, 2, 2, 0);
    SetScopedTextureBinderExpectations(GL_TEXTURE_2D);
    auto& cmd = *GetImmediateAs<cmds::CopySubTextureINTERNALImmediate>();
    cmd.Init(0, 0, 0, 0, 2, 1, mailboxes);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailboxes)));
  }

  // This will initialize bottom right corner of the destination.
  // CopySubTexture will clear the bottom half of the destination because a
  // single rectangle is insufficient to keep track of the initialized area.
  {
    SetupClearTextureExpectations(kServiceTextureId, kServiceTextureId,
                                  GL_TEXTURE_2D, GL_TEXTURE_2D, 0, GL_RGBA,
                                  GL_UNSIGNED_BYTE, 0, 1, 2, 1, 0);
    SetScopedTextureBinderExpectations(GL_TEXTURE_2D);
    auto& cmd = *GetImmediateAs<cmds::CopySubTextureINTERNALImmediate>();
    cmd.Init(1, 1, 0, 0, 1, 1, mailboxes);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailboxes)));
  }

  SharedImageRepresentationFactory repr_factory(shared_image_manager(),
                                                nullptr);
  auto representation = repr_factory.ProduceGLTexture(client_texture_mailbox_);
  EXPECT_TRUE(representation->GetTexture()->SafeToRenderFrom());
}

TEST_P(RasterDecoderManualInitTest, CopyTexSubImage2DValidateColorFormat) {
  InitState init;
  init.gl_version = "3.0";
  init.extensions.push_back("GL_EXT_texture_rg");
  InitDecoder(init);

  // Create dest texture.
  gpu::Mailbox dest_texture_mailbox =
      CreateFakeTexture(kNewServiceId, viz::ResourceFormat::RED_8,
                        /*width=*/2, /*height=*/2, /*cleared=*/true);

  auto& copy_cmd = *GetImmediateAs<cmds::CopySubTextureINTERNALImmediate>();
  GLbyte mailboxes[sizeof(gpu::Mailbox) * 2];
  CopyMailboxes(mailboxes, client_texture_mailbox_, dest_texture_mailbox);
  copy_cmd.Init(0, 0, 0, 0, 2, 1, mailboxes);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(copy_cmd, sizeof(mailboxes)));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

class RasterDecoderOOPTest : public testing::Test, DecoderClient {
 public:
  void SetUp() override {
    gl::GLSurfaceTestSupport::InitializeOneOff();
    gpu::GpuDriverBugWorkarounds workarounds;

    scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();
    scoped_refptr<gl::GLSurface> surface =
        gl::init::CreateOffscreenGLSurface(gfx::Size());
    scoped_refptr<gl::GLContext> context = gl::init::CreateGLContext(
        share_group.get(), surface.get(), gl::GLContextAttribs());
    ASSERT_TRUE(context->MakeCurrent(surface.get()));

    gpu_feature_info_.status_values[GPU_FEATURE_TYPE_OOP_RASTERIZATION] =
        kGpuFeatureStatusEnabled;
    auto feature_info = base::MakeRefCounted<gles2::FeatureInfo>(
        workarounds, gpu_feature_info_);

    context_state_ = base::MakeRefCounted<SharedContextState>(
        std::move(share_group), std::move(surface), std::move(context),
        false /* use_virtualized_gl_contexts */, base::DoNothing(),
        GpuPreferences().gr_context_type);
    context_state_->InitializeGrContext(workarounds, nullptr);
    context_state_->InitializeGL(GpuPreferences(), feature_info);
  }
  void TearDown() override {
    context_state_->MakeCurrent(nullptr);
    context_state_ = nullptr;
    gl::init::ShutdownGL(false);
  }

  // DecoderClient implementation.
  void OnConsoleMessage(int32_t id, const std::string& message) override {}
  void CacheShader(const std::string& key, const std::string& shader) override {
  }
  void OnFenceSyncRelease(uint64_t release) override {}
  void OnDescheduleUntilFinished() override {}
  void OnRescheduleAfterFinished() override {}
  void OnSwapBuffers(uint64_t swap_id, uint32_t flags) override {}
  void ScheduleGrContextCleanup() override {}
  void HandleReturnData(base::span<const uint8_t> data) override {}

  std::unique_ptr<RasterDecoder> CreateDecoder() {
    auto decoder = base::WrapUnique(RasterDecoder::Create(
        this, &command_buffer_service_, &outputter_, gpu_feature_info_,
        GpuPreferences(), nullptr /* memory_tracker */, &shared_image_manager_,
        context_state_));
    ContextCreationAttribs attribs;
    attribs.enable_oop_rasterization = true;
    attribs.enable_raster_interface = true;
    CHECK_EQ(decoder->Initialize(context_state_->surface(),
                                 context_state_->context(), true,
                                 gles2::DisallowedFeatures(), attribs),
             ContextResult::kSuccess);
    return decoder;
  }

  template <typename T>
  error::Error ExecuteCmd(RasterDecoder* decoder, const T& cmd) {
    static_assert(T::kArgFlags == cmd::kFixed,
                  "T::kArgFlags should equal cmd::kFixed");
    int entries_processed = 0;
    return decoder->DoCommands(1, (const void*)&cmd,
                               ComputeNumEntries(sizeof(cmd)),
                               &entries_processed);
  }

 protected:
  GpuFeatureInfo gpu_feature_info_;
  gles2::TraceOutputter outputter_;
  FakeCommandBufferServiceBase command_buffer_service_;
  scoped_refptr<SharedContextState> context_state_;

  SharedImageManager shared_image_manager_;
};

TEST_F(RasterDecoderOOPTest, StateRestoreAcrossDecoders) {
  // First decoder receives a skia command requiring context state reset.
  auto decoder1 = CreateDecoder();
  EXPECT_FALSE(context_state_->need_context_state_reset());
  decoder1->SetUpForRasterCHROMIUMForTest();
  cmds::EndRasterCHROMIUM end_raster_cmd;
  end_raster_cmd.Init();
  EXPECT_FALSE(error::IsError(ExecuteCmd(decoder1.get(), end_raster_cmd)));
  EXPECT_TRUE(context_state_->need_context_state_reset());

  // Another decoder receives a command which does not require consistent state,
  // it should be processed without state restoration.
  auto decoder2 = CreateDecoder();
  decoder2->SetUpForRasterCHROMIUMForTest();
  EXPECT_FALSE(error::IsError(ExecuteCmd(decoder2.get(), end_raster_cmd)));
  EXPECT_TRUE(context_state_->need_context_state_reset());

  decoder1->Destroy(true);
  context_state_->MakeCurrent(nullptr);
  decoder2->Destroy(true);

  // Make sure the context is preserved across decoders.
  EXPECT_FALSE(context_state_->gr_context()->abandoned());
}

}  // namespace raster
}  // namespace gpu
