// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/raster_decoder_unittest_base.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state_test_helpers.h"
#include "gpu/command_buffer/service/copy_texture_chromium_mock.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_factory.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/vertex_attrib_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

using ::gl::MockGLInterface;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::Between;
using ::testing::InSequence;
using ::testing::Ne;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

namespace gpu {
namespace raster {

RasterDecoderTestBase::InitState::InitState() = default;
RasterDecoderTestBase::InitState::~InitState() = default;

RasterDecoderTestBase::RasterDecoderTestBase()
    : surface_(nullptr),
      context_(nullptr),
      shared_memory_id_(0),
      shared_memory_offset_(0),
      shared_memory_address_(nullptr),
      shared_memory_base_(nullptr),
      ignore_cached_state_for_test_(GetParam()),
      memory_tracker_(nullptr) {
  memset(immediate_buffer_, 0xEE, sizeof(immediate_buffer_));
}

RasterDecoderTestBase::~RasterDecoderTestBase() = default;

void RasterDecoderTestBase::OnConsoleMessage(int32_t id,
                                             const std::string& message) {}
void RasterDecoderTestBase::CacheBlob(gpu::GpuDiskCacheType type,
                                      const std::string& key,
                                      const std::string& blob) {}
void RasterDecoderTestBase::OnFenceSyncRelease(uint64_t release) {}
void RasterDecoderTestBase::OnDescheduleUntilFinished() {}
void RasterDecoderTestBase::OnRescheduleAfterFinished() {}
void RasterDecoderTestBase::OnSwapBuffers(uint64_t swap_id, uint32_t flags) {}
bool RasterDecoderTestBase::ShouldYield() {
  return false;
}

void RasterDecoderTestBase::SetUp() {
  InitDecoder(InitState());
}

void RasterDecoderTestBase::AddExpectationsForGetCapabilities() {
  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_TEXTURE_SIZE, _))
      .WillOnce(SetArgPointee<1>(8192u))
      .RetiresOnSaturation();
}

void RasterDecoderTestBase::InitDecoder(const InitState& init) {
  std::string all_extensions;
  for (const std::string& extension : init.extensions) {
    all_extensions += extension + " ";
  }
  const ContextType context_type = init.context_type;

  // For easier substring/extension matching
  gl::SetGLGetProcAddressProc(gl::MockGLInterface::GetGLProcAddress);
  display_ = gl::GLSurfaceTestSupport::InitializeOneOffWithMockBindings();

  gl_ = std::make_unique<StrictMock<MockGLInterface>>();
  ::gl::MockGLInterface::SetGLInterface(gl_.get());

  InSequence sequence;

  surface_ = new gl::GLSurfaceStub;

  // Context needs to be created before initializing ContextGroup, which willxo
  // in turn initialize FeatureInfo, which needs a context to determine
  // extension support.
  context_ = new StrictMock<GLContextMock>();
  // The stub ctx needs to be initialized so that the gl::GLContext can
  // store the offscreen stub |surface|.
  context_->Initialize(surface_.get(), {});
  context_->SetExtensionsString(all_extensions.c_str());
  context_->SetGLVersionString(init.gl_version.c_str());

  context_->GLContextStub::MakeCurrentImpl(surface_.get());

  GpuFeatureInfo gpu_feature_info;
  feature_info_ = base::MakeRefCounted<gles2::FeatureInfo>(init.workarounds,
                                                           gpu_feature_info);
  gles2::TestHelper::SetupFeatureInfoInitExpectationsWithGLVersion(
      gl_.get(), all_extensions.c_str(), "", init.gl_version.c_str(),
      context_type);
  feature_info_->Initialize(context_type,
                            gpu_preferences_.use_passthrough_cmd_decoder &&
                                gles2::PassthroughCommandDecoderSupported(),
                            gles2::DisallowedFeatures());

  // Setup expectations for SharedContextState::InitializeGL().
  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_VERTEX_ATTRIBS, _))
      .WillOnce(SetArgPointee<1>(8u))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, _))
      .WillOnce(SetArgPointee<1>(8u))
      .RetiresOnSaturation();
  ContextStateTestHelpers::SetupInitState(gl_.get(), feature_info(),
                                          gfx::Size(1, 1));

  if (context_->HasRobustness()) {
    EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
        .WillOnce(Return(GL_NO_ERROR));
  }

  shared_context_state_ = base::MakeRefCounted<SharedContextState>(
      new gl::GLShareGroup(), surface_, context_,
      feature_info()->workarounds().use_virtualized_gl_contexts,
      base::DoNothing(), GpuPreferences().gr_context_type);
  shared_context_state_->disable_check_reset_status_throttling_for_test_ = true;
  shared_context_state_->InitializeGL(GpuPreferences(), feature_info_);

  command_buffer_service_ = std::make_unique<FakeCommandBufferServiceBase>();

  decoder_.reset(RasterDecoder::Create(
      this, command_buffer_service_.get(), &outputter_, gpu_feature_info,
      gpu_preferences_, nullptr /* memory_tracker */, &shared_image_manager_,
      shared_context_state_, true /* is_privileged */));
  decoder_->SetIgnoreCachedStateForTest(ignore_cached_state_for_test_);
  decoder_->DisableFlushWorkaroundForTest();
  decoder_->GetLogger()->set_log_synthesized_gl_errors(false);

  ContextCreationAttribs attribs;
  attribs.lose_context_when_out_of_memory =
      init.lose_context_when_out_of_memory;
  attribs.context_type = context_type;

  ASSERT_EQ(decoder_->Initialize(surface_, shared_context_state_->context(),
                                 true, gles2::DisallowedFeatures(), attribs),
            gpu::ContextResult::kSuccess);

  EXPECT_CALL(*context_, MakeCurrentImpl(surface_.get()))
      .WillOnce(Return(true));
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  if (context_->HasRobustness()) {
    EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
        .WillOnce(Return(GL_NO_ERROR));
  }

  decoder_->MakeCurrent();
  decoder_->BeginDecoding();

  scoped_refptr<gpu::Buffer> buffer =
      command_buffer_service_->CreateTransferBufferHelper(kSharedBufferSize,
                                                          &shared_memory_id_);
  shared_memory_offset_ = kSharedMemoryOffset;
  shared_memory_address_ =
      static_cast<int8_t*>(buffer->memory()) + shared_memory_offset_;
  shared_memory_base_ = buffer->memory();
  ClearSharedMemory();
}

void RasterDecoderTestBase::ResetDecoder() {
  if (!decoder_.get())
    return;
  // All Tests should have read all their GLErrors before getting here.
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  decoder_->EndDecoding();

  decoder_->Destroy(!decoder_->WasContextLost());
  decoder_.reset();
  command_buffer_service_.reset();
  context_->GLContextStub::MakeCurrentImpl(surface_.get());
  shared_context_state_.reset();
  ::gl::MockGLInterface::SetGLInterface(nullptr);
  gl_.reset();
  gl::GLSurfaceTestSupport::ShutdownGL(display_);
}

void RasterDecoderTestBase::TearDown() {
  ResetDecoder();
}

GLint RasterDecoderTestBase::GetGLError() {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::GetError cmd;
  cmd.Init(shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  return static_cast<GLint>(*GetSharedMemoryAs<GLenum*>());
}

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const size_t RasterDecoderTestBase::kSharedBufferSize;
const uint32_t RasterDecoderTestBase::kSharedMemoryOffset;
const int32_t RasterDecoderTestBase::kInvalidSharedMemoryId;
const uint32_t RasterDecoderTestBase::kInvalidSharedMemoryOffset;
const uint8_t RasterDecoderTestBase::kInitialMemoryValue;

const uint32_t RasterDecoderTestBase::kNewClientId;
#endif

}  // namespace raster
}  // namespace gpu
