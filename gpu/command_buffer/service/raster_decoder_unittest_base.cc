// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/raster_decoder_unittest_base.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state_test_helpers.h"
#include "gpu/command_buffer/service/copy_texture_chromium_mock.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing_factory_gl_texture.h"
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
      memory_tracker_(nullptr),
      copy_texture_manager_(nullptr) {
  memset(immediate_buffer_, 0xEE, sizeof(immediate_buffer_));
}

RasterDecoderTestBase::~RasterDecoderTestBase() = default;

void RasterDecoderTestBase::OnConsoleMessage(int32_t id,
                                             const std::string& message) {}
void RasterDecoderTestBase::CacheShader(const std::string& key,
                                        const std::string& shader) {}
void RasterDecoderTestBase::OnFenceSyncRelease(uint64_t release) {}
void RasterDecoderTestBase::OnDescheduleUntilFinished() {}
void RasterDecoderTestBase::OnRescheduleAfterFinished() {}
void RasterDecoderTestBase::OnSwapBuffers(uint64_t swap_id, uint32_t flags) {}

void RasterDecoderTestBase::SetUp() {
  InitDecoder(InitState());
}

void RasterDecoderTestBase::AddExpectationsForRestoreAttribState(
    GLuint attrib) {
  EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, _))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, VertexAttribPointer(attrib, _, _, _, _, _))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, VertexAttribDivisorANGLE(attrib, _))
      .Times(testing::AtMost(1))
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, _))
      .Times(1)
      .RetiresOnSaturation();

  if (attrib != 0 || feature_info()->gl_version_info().is_es) {
    // TODO(bajones): Not sure if I can tell which of these will be called
    EXPECT_CALL(*gl_, EnableVertexAttribArray(attrib))
        .Times(testing::AtMost(1))
        .RetiresOnSaturation();

    EXPECT_CALL(*gl_, DisableVertexAttribArray(attrib))
        .Times(testing::AtMost(1))
        .RetiresOnSaturation();
  }
}

gpu::Mailbox RasterDecoderTestBase::CreateFakeTexture(
    GLuint service_id,
    viz::ResourceFormat resource_format,
    GLsizei width,
    GLsizei height,
    bool cleared) {
  gpu::Mailbox mailbox = gpu::Mailbox::GenerateForSharedImage();
  std::unique_ptr<SharedImageBacking> backing =
      SharedImageBackingFactoryGLTexture::CreateSharedImageForTest(
          mailbox, GL_TEXTURE_2D, service_id, cleared, resource_format,
          gfx::Size(width, height), SHARED_IMAGE_USAGE_RASTER);
  shared_images_.push_back(
      shared_image_manager_.Register(std::move(backing), &memory_tracker_));
  return mailbox;
}

void RasterDecoderTestBase::InitDecoder(const InitState& init) {
  std::string all_extensions;
  for (const std::string& extension : init.extensions) {
    all_extensions += extension + " ";
  }
  const ContextType context_type = CONTEXT_TYPE_OPENGLES2;

  // For easier substring/extension matching
  gl::SetGLGetProcAddressProc(gl::MockGLInterface::GetGLProcAddress);
  gl::GLSurfaceTestSupport::InitializeOneOffWithMockBindings();

  gl_.reset(new StrictMock<MockGLInterface>());
  ::gl::MockGLInterface::SetGLInterface(gl_.get());

  InSequence sequence;

  surface_ = new gl::GLSurfaceStub;

  // Context needs to be created before initializing ContextGroup, which willxo
  // in turn initialize FeatureInfo, which needs a context to determine
  // extension support.
  context_ = new StrictMock<GLContextMock>();
  context_->SetExtensionsString(all_extensions.c_str());
  context_->SetGLVersionString(init.gl_version.c_str());

  context_->GLContextStub::MakeCurrent(surface_.get());

  GpuFeatureInfo gpu_feature_info;
  feature_info_ = base::MakeRefCounted<gles2::FeatureInfo>(init.workarounds,
                                                           gpu_feature_info);
  gles2::TestHelper::SetupFeatureInfoInitExpectationsWithGLVersion(
      gl_.get(), all_extensions.c_str(), "", init.gl_version.c_str(),
      context_type);
  feature_info_->Initialize(gpu::CONTEXT_TYPE_OPENGLES2,
                            gpu_preferences_.use_passthrough_cmd_decoder &&
                                gles2::PassthroughCommandDecoderSupported(),
                            gles2::DisallowedFeatures());

  // Setup expectations for SharedContextState::InitializeGL().
  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_VERTEX_ATTRIBS, _))
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

  shared_context_state_->InitializeGL(GpuPreferences(), feature_info_);

  command_buffer_service_.reset(new FakeCommandBufferServiceBase());

  decoder_.reset(RasterDecoder::Create(
      this, command_buffer_service_.get(), &outputter_, gpu_feature_info,
      gpu_preferences_, nullptr /* memory_tracker */, &shared_image_manager_,
      shared_context_state_));
  decoder_->SetIgnoreCachedStateForTest(ignore_cached_state_for_test_);
  decoder_->DisableFlushWorkaroundForTest();
  decoder_->GetLogger()->set_log_synthesized_gl_errors(false);

  copy_texture_manager_ = new gles2::MockCopyTextureResourceManager();
  decoder_->SetCopyTextureResourceManagerForTest(copy_texture_manager_);

  ContextCreationAttribs attribs;
  attribs.lose_context_when_out_of_memory =
      init.lose_context_when_out_of_memory;
  attribs.context_type = context_type;

  ASSERT_EQ(decoder_->Initialize(surface_, shared_context_state_->context(),
                                 true, gles2::DisallowedFeatures(), attribs),
            gpu::ContextResult::kSuccess);

  EXPECT_CALL(*context_, MakeCurrent(surface_.get())).WillOnce(Return(true));
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

  client_texture_mailbox_ = CreateFakeTexture(
      kServiceTextureId, viz::ResourceFormat::RGBA_8888, /*width=*/2,
      /*height=*/2, /*cleared=*/false);
}

void RasterDecoderTestBase::ResetDecoder() {
  if (!decoder_.get())
    return;
  // All Tests should have read all their GLErrors before getting here.
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  decoder_->EndDecoding();

  if (!decoder_->WasContextLost()) {
    EXPECT_CALL(*copy_texture_manager_, Destroy())
        .Times(1)
        .RetiresOnSaturation();
    copy_texture_manager_ = nullptr;
  }

  decoder_->Destroy(!decoder_->WasContextLost());
  decoder_.reset();
  command_buffer_service_.reset();
  for (auto& image : shared_images_)
    image->OnContextLost();
  shared_images_.clear();
  context_->GLContextStub::MakeCurrent(surface_.get());
  shared_context_state_.reset();
  ::gl::MockGLInterface::SetGLInterface(nullptr);
  gl_.reset();
  gl::init::ShutdownGL(false);
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

void RasterDecoderTestBase::SetBucketData(uint32_t bucket_id,
                                          const void* data,
                                          uint32_t data_size) {
  DCHECK(data || data_size == 0);
  cmd::SetBucketSize cmd1;
  cmd1.Init(bucket_id, data_size);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
  if (data) {
    memcpy(shared_memory_address_, data, data_size);
    cmd::SetBucketData cmd2;
    cmd2.Init(bucket_id, 0, data_size, shared_memory_id_, kSharedMemoryOffset);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
    ClearSharedMemory();
  }
}

void RasterDecoderTestBase::SetBucketAsCString(uint32_t bucket_id,
                                               const char* str) {
  SetBucketData(bucket_id, str, str ? (strlen(str) + 1) : 0);
}

void RasterDecoderTestBase::SetBucketAsCStrings(uint32_t bucket_id,
                                                GLsizei count,
                                                const char** str,
                                                GLsizei count_in_header,
                                                char str_end) {
  uint32_t header_size = sizeof(GLint) * (count + 1);
  uint32_t total_size = header_size;
  std::unique_ptr<GLint[]> header(new GLint[count + 1]);
  header[0] = static_cast<GLint>(count_in_header);
  for (GLsizei ii = 0; ii < count; ++ii) {
    header[ii + 1] = str && str[ii] ? strlen(str[ii]) : 0;
    total_size += header[ii + 1] + 1;
  }
  cmd::SetBucketSize cmd1;
  cmd1.Init(bucket_id, total_size);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
  memcpy(shared_memory_address_, header.get(), header_size);
  uint32_t offset = header_size;
  for (GLsizei ii = 0; ii < count; ++ii) {
    if (str && str[ii]) {
      size_t str_len = strlen(str[ii]);
      memcpy(static_cast<char*>(shared_memory_address_) + offset, str[ii],
             str_len);
      offset += str_len;
    }
    memcpy(static_cast<char*>(shared_memory_address_) + offset, &str_end, 1);
    offset += 1;
  }
  cmd::SetBucketData cmd2;
  cmd2.Init(bucket_id, 0, total_size, shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  ClearSharedMemory();
}

void RasterDecoderTestBase::SetScopedTextureBinderExpectations(GLenum target) {
  // ScopedTextureBinder
  EXPECT_CALL(*gl_, ActiveTexture(_)).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindTexture(target, Ne(0U))).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindTexture(target, 0)).Times(1).RetiresOnSaturation();
}

void RasterDecoderTestBase::SetupClearTextureExpectations(
    GLuint service_id,
    GLuint old_service_id,
    GLenum bind_target,
    GLenum target,
    GLint level,
    GLenum format,
    GLenum type,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLuint bound_pixel_unpack_buffer) {
  SetScopedTextureBinderExpectations(bind_target);
  EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_ALIGNMENT, _))
      .Times(1)
      .RetiresOnSaturation();
  if (bound_pixel_unpack_buffer) {
    EXPECT_CALL(*gl_, BindBuffer(GL_PIXEL_UNPACK_BUFFER, _))
        .Times(2)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_ROW_LENGTH, _))
        .Times(2)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_IMAGE_HEIGHT, _))
        .Times(2)
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, TexSubImage2D(target, level, xoffset, yoffset, width,
                                  height, format, type, _))
      .Times(1)
      .RetiresOnSaturation();
#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
#endif
}

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLint RasterDecoderTestBase::kMaxTextureSize;
const GLint RasterDecoderTestBase::kNumTextureUnits;

const GLuint RasterDecoderTestBase::kServiceBufferId;
const GLuint RasterDecoderTestBase::kServiceTextureId;
const GLuint RasterDecoderTestBase::kServiceVertexArrayId;

const size_t RasterDecoderTestBase::kSharedBufferSize;
const uint32_t RasterDecoderTestBase::kSharedMemoryOffset;
const int32_t RasterDecoderTestBase::kInvalidSharedMemoryId;
const uint32_t RasterDecoderTestBase::kInvalidSharedMemoryOffset;
const uint32_t RasterDecoderTestBase::kInitialResult;
const uint8_t RasterDecoderTestBase::kInitialMemoryValue;

const uint32_t RasterDecoderTestBase::kNewClientId;
const uint32_t RasterDecoderTestBase::kNewServiceId;
const uint32_t RasterDecoderTestBase::kInvalidClientId;
#endif

}  // namespace raster
}  // namespace gpu
