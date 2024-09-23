// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/raster_decoder.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/raster_decoder_unittest_base.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
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

TEST_P(RasterDecoderManualInitTest, GetCapabilitiesHalfFloatLinear) {
  InitState init;
  init.extensions.push_back("GL_OES_texture_half_float_linear");
  InitDecoder(init);
  AddExpectationsForGetCapabilities();
  const auto& caps = decoder_->GetCapabilities();
  EXPECT_TRUE(caps.texture_half_float_linear);
}

TEST_P(RasterDecoderManualInitTest, GetCapabilitiesNorm16) {
  // R16 requires an ES3 context plus the extension to be available.
  InitState init;
  init.context_type = CONTEXT_TYPE_OPENGLES3;
  init.gl_version = "OpenGL ES 3.0";
  init.extensions.push_back("GL_EXT_texture_norm16");
  InitDecoder(init);
  AddExpectationsForGetCapabilities();
  const auto& caps = decoder_->GetCapabilities();
  EXPECT_TRUE(caps.texture_norm16);
}

class RasterDecoderOOPTest : public testing::Test, DecoderClient {
 public:
  void SetUp() override {
    display_ = gl::GLSurfaceTestSupport::InitializeOneOff();
    gpu::GpuDriverBugWorkarounds workarounds;

    scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();
    scoped_refptr<gl::GLSurface> surface =
        gl::init::CreateOffscreenGLSurface(display_, gfx::Size());
    scoped_refptr<gl::GLContext> context = gl::init::CreateGLContext(
        share_group.get(), surface.get(), gl::GLContextAttribs());
    ASSERT_TRUE(context->MakeCurrent(surface.get()));

    gpu_feature_info_.status_values[GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION] =
        kGpuFeatureStatusEnabled;
    auto feature_info = base::MakeRefCounted<gles2::FeatureInfo>(
        workarounds, gpu_feature_info_);

    context_state_ = base::MakeRefCounted<SharedContextState>(
        std::move(share_group), std::move(surface), std::move(context),
        false /* use_virtualized_gl_contexts */, base::DoNothing(),
        GpuPreferences().gr_context_type);
    context_state_->InitializeSkia(GpuPreferences(), workarounds);
    context_state_->InitializeGL(GpuPreferences(), feature_info);

    decoder_ = CreateDecoder();

    scoped_refptr<gpu::Buffer> buffer =
        command_buffer_service_->CreateTransferBufferHelper(kSharedBufferSize,
                                                            &shared_memory_id_);
    shared_memory_offset_ = kSharedMemoryOffset;
    shared_memory_address_ =
        static_cast<int8_t*>(buffer->memory()) + shared_memory_offset_;

    workarounds.webgl_or_caps_max_texture_size = INT_MAX - 1;
    shared_image_factory_ = std::make_unique<SharedImageFactory>(
        GpuPreferences(), workarounds, GpuFeatureInfo(), context_state_.get(),
        &shared_image_manager_, nullptr,
        /*is_for_display_compositor=*/false);

    client_texture_mailbox_ =
        CreateMailbox(viz::SinglePlaneFormat::kRGBA_8888, /*width=*/2,
                      /*height=*/2, /*cleared=*/false);

    // When creating the mailbox, we create a WrappedSkImage shared image which
    // sets this flag to true. Some tests expect this flag to be false when
    // testing so we reset it back here to false.
    context_state_->set_need_context_state_reset(/*reset=*/false);
  }
  void TearDown() override {
    context_state_->MakeCurrent(nullptr);
    decoder_->EndDecoding();
    decoder_->Destroy(!decoder_->WasContextLost());
    decoder_.reset();

    command_buffer_service_.reset();
    shared_image_factory_->DestroyAllSharedImages(true);
    shared_image_factory_.reset();

    context_state_.reset();
    context_state_ = nullptr;
    gl::GLSurfaceTestSupport::ShutdownGL(display_);
  }

  RasterDecoderOOPTest() : memory_tracker_(nullptr) {
    memset(immediate_buffer_, 0xEE, sizeof(immediate_buffer_));
  }

  // DecoderClient implementation.
  void OnConsoleMessage(int32_t id, const std::string& message) override {}
  void CacheBlob(gpu::GpuDiskCacheType type,
                 const std::string& key,
                 const std::string& blob) override {}
  void OnFenceSyncRelease(uint64_t release) override {}
  void OnDescheduleUntilFinished() override {}
  void OnRescheduleAfterFinished() override {}
  void OnSwapBuffers(uint64_t swap_id, uint32_t flags) override {}
  void ScheduleGrContextCleanup() override {}
  void HandleReturnData(base::span<const uint8_t> data) override {}
  bool ShouldYield() override { return false; }

  std::unique_ptr<RasterDecoder> CreateDecoder() {
    command_buffer_service_ = std::make_unique<FakeCommandBufferServiceBase>();
    auto decoder = base::WrapUnique(RasterDecoder::Create(
        this, command_buffer_service_.get(), &outputter_, gpu_feature_info_,
        GpuPreferences(), nullptr /* memory_tracker */, &shared_image_manager_,
        context_state_, true /* is_privileged */));
    ContextCreationAttribs attribs;
    attribs.enable_oop_rasterization = true;
    attribs.enable_raster_interface = true;
    CHECK_EQ(decoder->Initialize(context_state_->surface(),
                                 context_state_->context(), true,
                                 gles2::DisallowedFeatures(), attribs),
             ContextResult::kSuccess);
    return decoder;
  }

  gpu::Mailbox CreateMailbox(viz::SharedImageFormat format,
                             GLsizei width,
                             GLsizei height,
                             bool cleared) {
    gpu::Mailbox mailbox = gpu::Mailbox::Generate();
    gfx::Size size(width, height);
    auto color_space = gfx::ColorSpace::CreateSRGB();

    // Via this function, this test creates mailboxes that are used as both the
    // sources of reads and destinations of writes via the raster interface.
    shared_image_factory_->CreateSharedImage(
        mailbox, format, size, color_space, kTopLeft_GrSurfaceOrigin,
        kPremul_SkAlphaType, gpu::kNullSurfaceHandle,
        SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE,
        "TestLabel");

    if (cleared) {
      SharedImageRepresentationFactory repr_factory(shared_image_manager(),
                                                    nullptr);
      auto representation =
          repr_factory.ProduceSkia(mailbox, context_state_.get());
      representation->SetCleared();
    }

    return mailbox;
  }

  template <typename T>
  T* GetImmediateAs() {
    return reinterpret_cast<T*>(immediate_buffer_);
  }

  template <typename T>
  error::Error ExecuteCmd(RasterDecoder* decoder, const T& cmd) {
    static_assert(T::kArgFlags == cmd::kFixed,
                  "T::kArgFlags should equal cmd::kFixed");
    int entries_processed = 0;
    return decoder->DoCommands(1, reinterpret_cast<const void*>(&cmd),
                               ComputeNumEntries(sizeof(cmd)),
                               &entries_processed);
  }

  template <typename T>
  error::Error ExecuteImmediateCmd(const T& cmd, size_t data_size) {
    static_assert(T::kArgFlags == cmd::kAtLeastN,
                  "T::kArgFlags should equal cmd::kAtLeastN");
    int entries_processed = 0;
    return decoder_->DoCommands(1, reinterpret_cast<const void*>(&cmd),
                                ComputeNumEntries(sizeof(cmd) + data_size),
                                &entries_processed);
  }

  template <typename T>
  T GetSharedMemoryAs() {
    return reinterpret_cast<T>(shared_memory_address_.get());
  }

  GLint GetGLError() {
    cmds::GetError cmd;
    cmd.Init(shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(decoder_.get(), cmd));
    return static_cast<GLint>(*GetSharedMemoryAs<GLenum*>());
  }

  SharedImageManager* shared_image_manager() { return &shared_image_manager_; }

 protected:
  GpuFeatureInfo gpu_feature_info_;
  gles2::TraceOutputter outputter_;
  std::unique_ptr<FakeCommandBufferServiceBase> command_buffer_service_;
  MemoryTypeTracker memory_tracker_;
  scoped_refptr<SharedContextState> context_state_;
  gpu::Mailbox client_texture_mailbox_;
  std::unique_ptr<RasterDecoder> decoder_;

  int32_t shared_memory_id_ = 0;
  uint32_t shared_memory_offset_ = 0;
  raw_ptr<void> shared_memory_address_ = nullptr;

  const size_t kSharedBufferSize = 2048;
  const uint32_t kSharedMemoryOffset = 132;

  uint32_t immediate_buffer_[64];

  std::unique_ptr<SharedImageFactory> shared_image_factory_;
  SharedImageManager shared_image_manager_;
  raw_ptr<gl::GLDisplay> display_ = nullptr;
};

TEST_F(RasterDecoderOOPTest, CopyTexSubImage2DSizeMismatch) {
  context_state_->set_need_context_state_reset(true);
  // Create uninitialized source texture mailbox.
  gpu::Mailbox source_texture_mailbox =
      CreateMailbox(viz::SinglePlaneFormat::kRGBA_8888,
                    /*width=*/1, /*height=*/1,
                    /*cleared=*/true);
  GLbyte mailboxes[sizeof(gpu::Mailbox) * 2];
  CopyMailboxes(mailboxes, source_texture_mailbox, client_texture_mailbox_);

  SharedImageRepresentationFactory repr_factory(shared_image_manager(),
                                                nullptr);
  auto representation =
      repr_factory.ProduceSkia(client_texture_mailbox_, context_state_.get());

  {
    // This will initialize the bottom right corner of destination.
    auto& cmd = *GetImmediateAs<cmds::CopySharedImageINTERNALImmediate>();
    cmd.Init(1, 1, 0, 0, 1, 1, false, mailboxes);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailboxes)));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_EQ(representation->ClearedRect(), gfx::Rect(1, 1, 1, 1));
  }

  {
    // Dest rect outside of dest bounds
    auto& cmd = *GetImmediateAs<cmds::CopySharedImageINTERNALImmediate>();
    cmd.Init(2, 2, 0, 0, 1, 1, false, mailboxes);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailboxes)));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
    EXPECT_EQ(representation->ClearedRect(), gfx::Rect(1, 1, 1, 1));
  }

  {
    // Source rect outside of source bounds
    auto& cmd = *GetImmediateAs<cmds::CopySharedImageINTERNALImmediate>();
    cmd.Init(0, 0, 0, 0, 2, 2, false, mailboxes);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailboxes)));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
    EXPECT_EQ(representation->ClearedRect(), gfx::Rect(1, 1, 1, 1));
  }
}

TEST_F(RasterDecoderOOPTest, CopyTexSubImage2DTwiceClearsUnclearedTexture) {
  context_state_->set_need_context_state_reset(true);
  // Create uninitialized source texture mailbox.
  gpu::Mailbox source_texture_mailbox =
      CreateMailbox(viz::SinglePlaneFormat::kRGBA_8888,
                    /*width=*/2, /*height=*/2,
                    /*cleared=*/true);
  GLbyte mailboxes[sizeof(gpu::Mailbox) * 2];
  CopyMailboxes(mailboxes, source_texture_mailbox, client_texture_mailbox_);

  SharedImageRepresentationFactory repr_factory(shared_image_manager(),
                                                nullptr);
  auto representation =
      repr_factory.ProduceSkia(client_texture_mailbox_, context_state_.get());
  EXPECT_FALSE(representation->IsCleared());

  // This will initialize the top half of destination.
  {
    auto& cmd = *GetImmediateAs<cmds::CopySharedImageINTERNALImmediate>();
    cmd.Init(0, 0, 0, 0, 2, 1, false, mailboxes);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailboxes)));
  }
  EXPECT_EQ(gfx::Rect(0, 0, 2, 1), representation->ClearedRect());
  EXPECT_FALSE(representation->IsCleared());

  // This will initialize bottom half of the destination.
  {
    auto& cmd = *GetImmediateAs<cmds::CopySharedImageINTERNALImmediate>();
    cmd.Init(0, 1, 0, 0, 2, 1, false, mailboxes);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailboxes)));
  }
  EXPECT_TRUE(representation->IsCleared());
}

// Unlike the GLES2 version, RasterInterface's CopySharedImage does not allow
// initializing a texture in parts *unless* the rectangles being cleared
// can be trivially combined into a larger rectangle.
TEST_F(RasterDecoderOOPTest, CopyTexSubImage2DPartialFailsWithUnalignedRect) {
  context_state_->set_need_context_state_reset(true);
  // Create uninitialized source texture mailbox.
  gpu::Mailbox source_texture_mailbox =
      CreateMailbox(viz::SinglePlaneFormat::kRGBA_8888,
                    /*width=*/2, /*height=*/2,
                    /*cleared=*/true);
  GLbyte mailboxes[sizeof(gpu::Mailbox) * 2];
  CopyMailboxes(mailboxes, source_texture_mailbox, client_texture_mailbox_);

  SharedImageRepresentationFactory repr_factory(shared_image_manager(),
                                                nullptr);
  auto representation =
      repr_factory.ProduceSkia(client_texture_mailbox_, context_state_.get());
  EXPECT_FALSE(representation->IsCleared());

  // This will initialize the top half of destination.
  {
    auto& cmd = *GetImmediateAs<cmds::CopySharedImageINTERNALImmediate>();
    cmd.Init(0, 0, 0, 0, 2, 1, false, mailboxes);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailboxes)));
  }
  EXPECT_EQ(gfx::Rect(0, 0, 2, 1), representation->ClearedRect());
  EXPECT_FALSE(representation->IsCleared());

  // This will attempt to initialize the bottom corner of the destination.  As
  // the new rect cannot be trivially combined with the previous cleared rect,
  // this will fail.
  {
    auto& cmd = *GetImmediateAs<cmds::CopySharedImageINTERNALImmediate>();
    cmd.Init(1, 1, 0, 0, 1, 1, false, mailboxes);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailboxes)));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  }
  EXPECT_EQ(gfx::Rect(0, 0, 2, 1), representation->ClearedRect());
  EXPECT_FALSE(representation->IsCleared());
}

TEST_F(RasterDecoderOOPTest, StateRestoreAcrossDecoders) {
  // First decoder receives a skia command requiring context state reset.
  auto decoder1 = CreateDecoder();
  EXPECT_FALSE(context_state_->need_context_state_reset());
  decoder1->MakeCurrent();
  decoder1->SetUpForRasterCHROMIUMForTest();
  cmds::EndRasterCHROMIUM end_raster_cmd;
  end_raster_cmd.Init();
  EXPECT_FALSE(error::IsError(ExecuteCmd(decoder1.get(), end_raster_cmd)));
  EXPECT_TRUE(context_state_->need_context_state_reset());

  // Another decoder receives a command which does not require consistent state,
  // it should be processed without state restoration.
  auto decoder2 = CreateDecoder();
  decoder2->MakeCurrent();
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
