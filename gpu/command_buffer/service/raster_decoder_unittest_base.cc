// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/raster_decoder_unittest_base.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/copy_texture_chromium_mock.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/raster_decoder_context_state.h"
#include "gpu/command_buffer/service/service_utils.h"
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
      client_texture_id_(106),
      shared_memory_id_(0),
      shared_memory_offset_(0),
      shared_memory_address_(nullptr),
      shared_memory_base_(nullptr),
      ignore_cached_state_for_test_(GetParam()),
      shader_translator_cache_(gpu_preferences_),
      copy_texture_manager_(nullptr) {
  memset(immediate_buffer_, 0xEE, sizeof(immediate_buffer_));
}

RasterDecoderTestBase::~RasterDecoderTestBase() = default;

void RasterDecoderTestBase::OnConsoleMessage(int32_t id,
                                             const std::string& message) {}
void RasterDecoderTestBase::CacheShader(const std::string& key,
                                        const std::string& shader) {}
void RasterDecoderTestBase::OnFenceSyncRelease(uint64_t release) {}
bool RasterDecoderTestBase::OnWaitSyncToken(const gpu::SyncToken&) {
  return false;
}
void RasterDecoderTestBase::OnDescheduleUntilFinished() {}
void RasterDecoderTestBase::OnRescheduleAfterFinished() {}
void RasterDecoderTestBase::OnSwapBuffers(uint64_t swap_id, uint32_t flags) {}

void RasterDecoderTestBase::SetUp() {
  InitDecoder(InitState());
}

void RasterDecoderTestBase::AddExpectationsForVertexAttribManager() {
  for (GLint ii = 0; ii < kNumVertexAttribs; ++ii) {
    EXPECT_CALL(*gl_, VertexAttrib4f(ii, 0.0f, 0.0f, 0.0f, 1.0f))
        .Times(1)
        .RetiresOnSaturation();
  }
}

void RasterDecoderTestBase::AddExpectationsForBindVertexArrayOES() {
  if (group_->feature_info()->feature_flags().native_vertex_array_object) {
    EXPECT_CALL(*gl_, BindVertexArrayOES(_)).Times(1).RetiresOnSaturation();
  } else {
    for (uint32_t vv = 0; vv < group_->max_vertex_attribs(); ++vv) {
      AddExpectationsForRestoreAttribState(vv);
    }

    EXPECT_CALL(*gl_, BindBuffer(GL_ELEMENT_ARRAY_BUFFER, _))
        .Times(1)
        .RetiresOnSaturation();
  }
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

  if (attrib != 0 || group_->feature_info()->gl_version_info().is_es) {
    // TODO(bajones): Not sure if I can tell which of these will be called
    EXPECT_CALL(*gl_, EnableVertexAttribArray(attrib))
        .Times(testing::AtMost(1))
        .RetiresOnSaturation();

    EXPECT_CALL(*gl_, DisableVertexAttribArray(attrib))
        .Times(testing::AtMost(1))
        .RetiresOnSaturation();
  }
}

void RasterDecoderTestBase::SetupInitStateManualExpectations(bool es3_capable) {
  if (es3_capable) {
    EXPECT_CALL(*gl_, PixelStorei(GL_PACK_ROW_LENGTH, 0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_ROW_LENGTH, 0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0))
        .Times(1)
        .RetiresOnSaturation();
    if (group_->feature_info()->feature_flags().ext_window_rectangles) {
      EXPECT_CALL(*gl_, WindowRectanglesEXT(GL_EXCLUSIVE_EXT, 0, nullptr))
          .Times(1)
          .RetiresOnSaturation();
    }
  }
}

void RasterDecoderTestBase::SetupInitStateManualExpectationsForDoLineWidth(
    GLfloat width) {
  EXPECT_CALL(*gl_, LineWidth(width)).Times(1).RetiresOnSaturation();
}

void RasterDecoderTestBase::ExpectEnableDisable(GLenum cap, bool enable) {
  if (enable) {
    EXPECT_CALL(*gl_, Enable(cap)).Times(1).RetiresOnSaturation();
  } else {
    EXPECT_CALL(*gl_, Disable(cap)).Times(1).RetiresOnSaturation();
  }
}

void RasterDecoderTestBase::InitDecoder(const InitState& init) {
  std::string all_extensions;
  for (const std::string& extension : init.extensions) {
    all_extensions += extension + " ";
  }
  const bool bind_generates_resource(false);
  const ContextType context_type(CONTEXT_TYPE_OPENGLES2);

  // For easier substring/extension matching
  gl::SetGLGetProcAddressProc(gl::MockGLInterface::GetGLProcAddress);
  gl::GLSurfaceTestSupport::InitializeOneOffWithMockBindings();

  gl_.reset(new StrictMock<MockGLInterface>());
  ::gl::MockGLInterface::SetGLInterface(gl_.get());

  GpuFeatureInfo gpu_feature_info;
  scoped_refptr<gles2::FeatureInfo> feature_info =
      new gles2::FeatureInfo(init.workarounds, gpu_feature_info);

  group_ = scoped_refptr<gles2::ContextGroup>(new gles2::ContextGroup(
      gpu_preferences_, false, &mailbox_manager_, nullptr /* memory_tracker */,
      &shader_translator_cache_, &framebuffer_completeness_cache_, feature_info,
      bind_generates_resource, &image_manager_, nullptr /* image_factory */,
      nullptr /* progress_reporter */, gpu_feature_info, &discardable_manager_,
      nullptr /* passthrough_discardable_manager */, &shared_image_manager_));

  InSequence sequence;

  surface_ = new gl::GLSurfaceStub;

  // Context needs to be created before initializing ContextGroup, which willxo
  // in turn initialize FeatureInfo, which needs a context to determine
  // extension support.
  context_ = new StrictMock<GLContextMock>();
  context_->SetExtensionsString(all_extensions.c_str());
  context_->SetGLVersionString(init.gl_version.c_str());

  context_->GLContextStub::MakeCurrent(surface_.get());

  gles2::TestHelper::SetupContextGroupInitExpectations(
      gl_.get(), gles2::DisallowedFeatures(), all_extensions.c_str(),
      init.gl_version.c_str(), context_type, bind_generates_resource);

  // We initialize the ContextGroup with a MockRasterDecoder so that
  // we can use the ContextGroup to figure out how the real RasterDecoder
  // will initialize itself.
  command_buffer_service_.reset(new FakeCommandBufferServiceBase());
  mock_decoder_.reset(new MockRasterDecoder(command_buffer_service_.get()));

  EXPECT_EQ(group_->Initialize(mock_decoder_.get(), context_type,
                               gles2::DisallowedFeatures()),
            gpu::ContextResult::kSuccess);

  scoped_refptr<gpu::Buffer> buffer =
      command_buffer_service_->CreateTransferBufferHelper(kSharedBufferSize,
                                                          &shared_memory_id_);
  shared_memory_offset_ = kSharedMemoryOffset;
  shared_memory_address_ =
      static_cast<int8_t*>(buffer->memory()) + shared_memory_offset_;
  shared_memory_base_ = buffer->memory();
  ClearSharedMemory();

  ContextCreationAttribs attribs;
  attribs.lose_context_when_out_of_memory =
      init.lose_context_when_out_of_memory;
  attribs.context_type = context_type;

  if (group_->feature_info()->feature_flags().native_vertex_array_object) {
    EXPECT_CALL(*gl_, GenVertexArraysOES(1, _))
        .WillOnce(SetArgPointee<1>(kServiceVertexArrayId))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindVertexArrayOES(_)).Times(1).RetiresOnSaturation();
  }

  if (group_->feature_info()->workarounds().init_vertex_attributes)
    AddExpectationsForVertexAttribManager();

  AddExpectationsForBindVertexArrayOES();

  bool use_default_textures = bind_generates_resource;
  for (GLint tt = 0; tt < gles2::TestHelper::kNumTextureUnits; ++tt) {
    EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0 + tt))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
                BindTexture(GL_TEXTURE_2D,
                            use_default_textures
                                ? gles2::TestHelper::kServiceDefaultTexture2dId
                                : 0))
        .Times(1)
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0)).Times(1).RetiresOnSaturation();

  SetupInitCapabilitiesExpectations(group_->feature_info()->IsES3Capable());
  SetupInitStateExpectations(group_->feature_info()->IsES3Capable());

  scoped_refptr<raster::RasterDecoderContextState> context_state =
      new raster::RasterDecoderContextState(
          new gl::GLShareGroup(), surface_, context_,
          feature_info->workarounds().use_virtualized_gl_contexts);
  decoder_.reset(RasterDecoder::Create(this, command_buffer_service_.get(),
                                       &outputter_, group_.get(),
                                       std::move(context_state)));
  decoder_->SetIgnoreCachedStateForTest(ignore_cached_state_for_test_);
  decoder_->GetLogger()->set_log_synthesized_gl_errors(false);

  copy_texture_manager_ = new gles2::MockCopyTextureResourceManager();
  decoder_->SetCopyTextureResourceManagerForTest(copy_texture_manager_);

  ASSERT_EQ(decoder_->Initialize(surface_, context_, true,
                                 gles2::DisallowedFeatures(), attribs),
            gpu::ContextResult::kSuccess);

  EXPECT_CALL(*context_, MakeCurrent(surface_.get())).WillOnce(Return(true));
  if (context_->WasAllocatedUsingRobustnessExtension()) {
    EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
        .WillOnce(Return(GL_NO_ERROR));
  }
  decoder_->MakeCurrent();
  decoder_->BeginDecoding();

  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kServiceTextureId))
      .RetiresOnSaturation();
  cmds::CreateTexture cmd;
  cmd.Init(false /* use_buffer */, gfx::BufferUsage::GPU_READ,
           viz::ResourceFormat::RGBA_8888, client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  EXPECT_EQ(GL_NO_ERROR, GetGLError());
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
  group_->Destroy(mock_decoder_.get(), false);
  command_buffer_service_.reset();
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

void RasterDecoderTestBase::DoDeleteTexture(GLuint client_id,
                                            GLuint service_id) {
  {
    InSequence s;

    // Calling DoDeleteTexture will unbind the texture from any texture units
    // it's currently bound to.
    EXPECT_CALL(*gl_, BindTexture(_, 0)).Times(AnyNumber());

    EXPECT_CALL(*gl_, DeleteTextures(1, Pointee(service_id)))
        .Times(1)
        .RetiresOnSaturation();

    GenHelper<cmds::DeleteTexturesImmediate>(client_id);
  }
}

void RasterDecoderTestBase::SetScopedTextureBinderExpectations(GLenum target) {
  // ScopedTextureBinder
  EXPECT_CALL(*gl_, ActiveTexture(_))
      .Times(Between(1, 2))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindTexture(target, Ne(0U))).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindTexture(target, 0)).Times(1).RetiresOnSaturation();
}

void RasterDecoderTestBase::DoTexStorage2D(GLuint client_id,
                                           GLsizei width,
                                           GLsizei height) {
  cmds::TexStorage2D tex_storage_cmd;
  tex_storage_cmd.Init(client_id, width, height);

  SetScopedTextureBinderExpectations(GL_TEXTURE_2D);

  if (decoder_->GetCapabilities().texture_storage) {
    EXPECT_CALL(*gl_,
                TexStorage2DEXT(GL_TEXTURE_2D, /*levels=*/1, _, width, height))
        .Times(1)
        .RetiresOnSaturation();
  } else {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
                TexImage2D(GL_TEXTURE_2D, _, _, width, height, _, _, _, _))
        .Times(1)
        .RetiresOnSaturation();
  }
  EXPECT_EQ(error::kNoError, ExecuteCmd(tex_storage_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
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
  EXPECT_CALL(*gl_, BindTexture(bind_target, service_id))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_ALIGNMENT, _))
      .Times(2)
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
  EXPECT_CALL(*gl_, BindTexture(bind_target, old_service_id))
      .Times(1)
      .RetiresOnSaturation();
#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
#endif
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/service/raster_decoder_unittest_0_autogen.h"

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLint RasterDecoderTestBase::kMaxTextureSize;
const GLint RasterDecoderTestBase::kNumTextureUnits;
const GLint RasterDecoderTestBase::kNumVertexAttribs;

const GLint RasterDecoderTestBase::kViewportX;
const GLint RasterDecoderTestBase::kViewportY;
const GLint RasterDecoderTestBase::kViewportWidth;
const GLint RasterDecoderTestBase::kViewportHeight;

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
