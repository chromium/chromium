// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_base.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/heap_array.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state_test_helpers.h"
#include "gpu/command_buffer/service/copy_texture_chromium_mock.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/vertex_attrib_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

using ::gl::MockGLInterface;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace {

void NormalizeInitState(gpu::gles2::GLES2DecoderTestBase::InitState* init) {
  CHECK(init);
  const char* kVAOExtensions[] = {
      "GL_OES_vertex_array_object",
      "GL_ARB_vertex_array_object",
      "GL_APPLE_vertex_array_object"
  };
  bool contains_vao_extension = false;
  for (const char* extension : kVAOExtensions) {
    if (base::Contains(init->extensions, extension)) {
      contains_vao_extension = true;
      break;
    }
  }

  if (init->use_native_vao) {
    if (contains_vao_extension)
      return;
    if (!init->extensions.empty())
      init->extensions += " ";
    if (base::StartsWith(init->gl_version, "opengl es",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      init->extensions += kVAOExtensions[0];
    } else {
#if !BUILDFLAG(IS_MAC)
      init->extensions += kVAOExtensions[1];
#else
      init->extensions += kVAOExtensions[2];
#endif  // BUILDFLAG(IS_MAC)
    }
  } else {
    // Make sure we don't set up an invalid InitState.
    CHECK(!contains_vao_extension);
  }

  if (!init->extensions.empty())
    init->extensions += " ";
  init->extensions += "GL_EXT_framebuffer_object ";
}

const uint32_t kMaxColorAttachments = 16;
const uint32_t kMaxDrawBuffers = 16;

}  // namespace

namespace gpu {
namespace gles2 {

GLES2DecoderTestBase::GLES2DecoderTestBase()
    : surface_(nullptr),
      context_(nullptr),
      client_buffer_id_(100),
      client_framebuffer_id_(101),
      client_program_id_(102),
      client_renderbuffer_id_(103),
      client_sampler_id_(104),
      client_shader_id_(105),
      client_texture_id_(106),
      client_element_buffer_id_(107),
      client_vertex_shader_id_(121),
      client_fragment_shader_id_(122),
      client_query_id_(123),
      client_vertexarray_id_(124),
      client_transformfeedback_id_(126),
      client_sync_id_(127),
      shared_memory_id_(0),
      shared_memory_offset_(0),
      shared_memory_address_(nullptr),
      shared_memory_base_(nullptr),
      service_renderbuffer_id_(0),
      service_renderbuffer_valid_(false),
      ignore_cached_state_for_test_(GetParam()),
      cached_color_mask_red_(true),
      cached_color_mask_green_(true),
      cached_color_mask_blue_(true),
      cached_color_mask_alpha_(true),
      cached_depth_mask_(true),
      cached_stencil_front_mask_(static_cast<GLuint>(-1)),
      cached_stencil_back_mask_(static_cast<GLuint>(-1)),
      shader_language_version_(100),
      shader_translator_cache_(gpu_preferences_),
      discardable_manager_(gpu_preferences_) {
  memset(immediate_buffer_, 0xEE, sizeof(immediate_buffer_));
}

GLES2DecoderTestBase::~GLES2DecoderTestBase() = default;

void GLES2DecoderTestBase::OnConsoleMessage(int32_t id,
                                            const std::string& message) {}
void GLES2DecoderTestBase::CacheBlob(gpu::GpuDiskCacheType type,
                                     const std::string& key,
                                     const std::string& blob) {}
void GLES2DecoderTestBase::OnFenceSyncRelease(uint64_t release) {}
void GLES2DecoderTestBase::OnDescheduleUntilFinished() {}
void GLES2DecoderTestBase::OnRescheduleAfterFinished() {}
void GLES2DecoderTestBase::OnSwapBuffers(uint64_t swap_id, uint32_t flags) {}
bool GLES2DecoderTestBase::ShouldYield() {
  return false;
}

void GLES2DecoderTestBase::SetUp() {
  InitState init;
  init.gl_version = "OpenGL ES 2.0";
  init.has_alpha = true;
  init.has_depth = true;
  init.request_alpha = true;
  init.request_depth = true;
  init.bind_generates_resource = true;
  InitDecoder(init);
}

GLES2DecoderTestBase::InitState::InitState() = default;
GLES2DecoderTestBase::InitState::InitState(const InitState& other) = default;
GLES2DecoderTestBase::InitState& GLES2DecoderTestBase::InitState::operator=(
    const InitState& other) = default;

void GLES2DecoderTestBase::InitDecoder(const InitState& init) {
  gpu::GpuDriverBugWorkarounds workarounds;
  InitDecoderWithWorkarounds(init, workarounds);
}

void GLES2DecoderTestBase::InitDecoderWithWorkarounds(
    const InitState& init,
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  ContextResult result = MaybeInitDecoderWithWorkarounds(init, workarounds);
  ASSERT_EQ(result, gpu::ContextResult::kSuccess);
}

ContextResult GLES2DecoderTestBase::MaybeInitDecoderWithWorkarounds(
    const InitState& init,
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  InitState normalized_init = init;
  NormalizeInitState(&normalized_init);
  // For easier substring/extension matching
  DCHECK(normalized_init.extensions.empty() ||
         *normalized_init.extensions.rbegin() == ' ');
  gl::SetGLGetProcAddressProc(gl::MockGLInterface::GetGLProcAddress);
  display_ = gl::GLSurfaceTestSupport::InitializeOneOffWithMockBindings();

  gl_ = std::make_unique<StrictMock<MockGLInterface>>();
  gl::MockGLInterface::SetGLInterface(gl_.get());

  SetupMockGLBehaviors();

  GpuFeatureInfo gpu_feature_info;
  scoped_refptr<FeatureInfo> feature_info =
      new FeatureInfo(workarounds, gpu_feature_info);

  group_ = scoped_refptr<ContextGroup>(new ContextGroup(
      gpu_preferences_, GetParam(), std::move(memory_tracker_),
      &shader_translator_cache_, &framebuffer_completeness_cache_, feature_info,
      normalized_init.bind_generates_resource, nullptr /* progress_reporter */,
      gpu_feature_info, &discardable_manager_, nullptr,
      &shared_image_manager_));
  bool use_default_textures = normalized_init.bind_generates_resource;

  InSequence sequence;

  surface_ = new gl::GLSurfaceStub;
  surface_->SetSize(gfx::Size(kBackBufferWidth, kBackBufferHeight));

  // Context needs to be created before initializing ContextGroup, which will
  // in turn initialize FeatureInfo, which needs a context to determine
  // extension support.
  context_ = new StrictMock<GLContextMock>();
  context_->SetExtensionsString(normalized_init.extensions.c_str());
  context_->SetGLVersionString(normalized_init.gl_version.c_str());

  context_->GLContextStub::MakeCurrentImpl(surface_.get());

  TestHelper::SetupContextGroupInitExpectations(
      gl_.get(),
      DisallowedFeatures(),
      normalized_init.extensions.c_str(),
      normalized_init.gl_version.c_str(),
      init.context_type,
      normalized_init.bind_generates_resource);

  // We initialize the ContextGroup with a MockGLES2Decoder so that
  // we can use the ContextGroup to figure out how the real GLES2Decoder
  // will initialize itself.
  command_buffer_service_ = std::make_unique<FakeCommandBufferServiceBase>();
  mock_decoder_ = std::make_unique<MockGLES2Decoder>(
      this, command_buffer_service_.get(), &outputter_);

  EXPECT_EQ(group_->Initialize(mock_decoder_.get(), init.context_type,
                               DisallowedFeatures()),
            gpu::ContextResult::kSuccess);

  // GPUTracer
  if (group_->feature_info()->feature_flags().ext_disjoint_timer_query) {
    EXPECT_CALL(*gl_, GetIntegerv(GL_GPU_DISJOINT_EXT, _))
        .WillOnce(SetArgPointee<1>(0))
        .RetiresOnSaturation();
  }

  if (init.context_type == CONTEXT_TYPE_WEBGL2 ||
      init.context_type == CONTEXT_TYPE_OPENGLES3) {
    EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_COLOR_ATTACHMENTS, _))
        .WillOnce(SetArgPointee<1>(kMaxColorAttachments))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_DRAW_BUFFERS, _))
        .WillOnce(SetArgPointee<1>(kMaxDrawBuffers))
        .RetiresOnSaturation();

    EXPECT_CALL(*gl_, GenTransformFeedbacks(1, _))
        .WillOnce(SetArgPointee<1>(kServiceDefaultTransformFeedbackId))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindTransformFeedback(GL_TRANSFORM_FEEDBACK,
                                            kServiceDefaultTransformFeedbackId))
        .Times(1)
        .RetiresOnSaturation();
  }

  if (group_->feature_info()->feature_flags().native_vertex_array_object) {
    EXPECT_CALL(*gl_, GenVertexArraysOES(1, _))
        .WillOnce(SetArgPointee<1>(kServiceVertexArrayId))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindVertexArrayOES(_)).Times(1).RetiresOnSaturation();
  }

  AddExpectationsForBindVertexArrayOES();

  // GLES2QueryManager
  if (group_->feature_info()->feature_flags().ext_disjoint_timer_query) {
    EXPECT_CALL(*gl_, GetIntegerv(GL_GPU_DISJOINT_EXT, _))
        .WillOnce(SetArgPointee<1>(0))
        .RetiresOnSaturation();
  }

  static GLuint attrib_0_id[] = {
    kServiceAttrib0BufferId,
  };
  static GLuint fixed_attrib_buffer_id[] = {
    kServiceFixedAttribBufferId,
  };
  EXPECT_CALL(*gl_, GenBuffersARB(std::size(attrib_0_id), _))
      .WillOnce(SetArrayArgument<1>(attrib_0_id,
                                    attrib_0_id + std::size(attrib_0_id)))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, kServiceAttrib0BufferId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, VertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, nullptr))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GenBuffersARB(std::size(fixed_attrib_buffer_id), _))
      .WillOnce(SetArrayArgument<1>(
          fixed_attrib_buffer_id,
          fixed_attrib_buffer_id + std::size(fixed_attrib_buffer_id)))
      .RetiresOnSaturation();

  for (GLint tt = 0; tt < TestHelper::kNumTextureUnits; ++tt) {
    EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0 + tt))
        .Times(1)
        .RetiresOnSaturation();
    if (group_->feature_info()->feature_flags().oes_egl_image_external ||
        group_->feature_info()
            ->feature_flags()
            .nv_egl_stream_consumer_external) {
      EXPECT_CALL(*gl_,
                  BindTexture(GL_TEXTURE_EXTERNAL_OES,
                              use_default_textures
                                  ? TestHelper::kServiceDefaultExternalTextureId
                                  : 0))
          .Times(1)
          .RetiresOnSaturation();
    }
    if (group_->feature_info()->feature_flags().arb_texture_rectangle) {
      EXPECT_CALL(
          *gl_,
          BindTexture(GL_TEXTURE_RECTANGLE_ARB,
                      use_default_textures
                          ? TestHelper::kServiceDefaultRectangleTextureId
                          : 0))
          .Times(1)
          .RetiresOnSaturation();
    }
    EXPECT_CALL(*gl_,
                BindTexture(GL_TEXTURE_CUBE_MAP,
                            use_default_textures
                                ? TestHelper::kServiceDefaultTextureCubemapId
                                : 0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(
        *gl_,
        BindTexture(
            GL_TEXTURE_2D,
            use_default_textures ? TestHelper::kServiceDefaultTexture2dId : 0))
        .Times(1)
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, BindFramebufferEXT(GL_FRAMEBUFFER, 0))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, GetIntegerv(GL_ALPHA_BITS, _))
      .WillOnce(SetArgPointee<1>(normalized_init.has_alpha ? 8 : 0))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_DEPTH_BITS, _))
      .WillOnce(SetArgPointee<1>(normalized_init.has_depth ? 24 : 0))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_STENCIL_BITS, _))
      .WillOnce(SetArgPointee<1>(normalized_init.has_stencil ? 8 : 0))
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_,
              GetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, _, _))
      .RetiresOnSaturation();

  static GLint max_viewport_dims[] = {
    kMaxViewportWidth,
    kMaxViewportHeight
  };
  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_VIEWPORT_DIMS, _))
      .WillOnce(SetArrayArgument<1>(
          max_viewport_dims, max_viewport_dims + std::size(max_viewport_dims)))
      .RetiresOnSaturation();

  static GLfloat line_width_range[] = { 1.0f, 2.0f };
  EXPECT_CALL(*gl_, GetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, _))
      .WillOnce(SetArrayArgument<1>(
          line_width_range, line_width_range + std::size(line_width_range)))
      .RetiresOnSaturation();

  if (group_->feature_info()->feature_flags().ext_window_rectangles) {
    static GLint max_window_rectangles = 4;
    EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_WINDOW_RECTANGLES_EXT, _))
        .WillOnce(SetArgPointee<1>(max_window_rectangles))
        .RetiresOnSaturation();
  }

  ContextStateTestHelpers::SetupInitState(
      gl_.get(), group_->feature_info(),
      gfx::Size(kBackBufferWidth, kBackBufferHeight));

  EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindFramebufferEXT(GL_FRAMEBUFFER, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindRenderbufferEXT(GL_RENDERBUFFER, 0))
      .Times(1)
      .RetiresOnSaturation();

  // TODO(boliu): Remove OS_ANDROID once crbug.com/259023 is fixed and the
  // workaround has been reverted.
#if !BUILDFLAG(IS_ANDROID)
  if (normalized_init.has_alpha && !normalized_init.request_alpha) {
    EXPECT_CALL(*gl_, ClearColor(0, 0, 0, 1)).Times(1).RetiresOnSaturation();
  }

  EXPECT_CALL(*gl_, Clear(
      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT))
      .Times(1)
      .RetiresOnSaturation();

  if (normalized_init.has_alpha && !normalized_init.request_alpha) {
    EXPECT_CALL(*gl_, ClearColor(0, 0, 0, 0)).Times(1).RetiresOnSaturation();
  }
#endif

  if (context_->HasRobustness()) {
    EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
        .WillOnce(Return(init.lose_context_on_init ? GL_GUILTY_CONTEXT_RESET_ARB
                                                   : GL_NO_ERROR));
  }

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
      normalized_init.lose_context_when_out_of_memory;
  attribs.context_type = init.context_type;

  decoder_.reset(GLES2Decoder::Create(this, command_buffer_service_.get(),
                                      &outputter_, group_.get()));
  decoder_->SetIgnoreCachedStateForTest(ignore_cached_state_for_test_);
  decoder_->GetLogger()->set_log_synthesized_gl_errors(false);

  copy_texture_manager_ = new MockCopyTextureResourceManager();
  decoder_->SetCopyTextureResourceManagerForTest(copy_texture_manager_);
  if (feature_info->gl_version_info().NeedsLuminanceAlphaEmulation()) {
    copy_tex_image_blitter_ =
        new MockCopyTexImageResourceManager(feature_info.get());
    decoder_->SetCopyTexImageBlitterForTest(copy_tex_image_blitter_);
  }
  gpu::ContextResult result = decoder_->Initialize(
      surface_, context_, false, DisallowedFeatures(), attribs);
  if (result != gpu::ContextResult::kSuccess) {
    // GLES2CmdDecoder::Destroy should be handled by Initialize in all failure
    // cases.
    decoder_.reset();
    group_->Destroy(mock_decoder_.get(), false);
    return result;
  }

  EXPECT_CALL(*context_, MakeCurrentImpl(surface_.get()))
      .WillOnce(Return(true));
  if (context_->HasRobustness()) {
    EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
        .WillOnce(Return(GL_NO_ERROR));
  }
  decoder_->MakeCurrent();
  decoder_->BeginDecoding();

  EXPECT_CALL(*gl_, GenBuffersARB(_, _))
      .WillOnce(SetArgPointee<1>(kServiceBufferId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenBuffersImmediate>(client_buffer_id_);
  EXPECT_CALL(*gl_, GenFramebuffersEXT(_, _))
      .WillOnce(SetArgPointee<1>(kServiceFramebufferId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenFramebuffersImmediate>(client_framebuffer_id_);
  EXPECT_CALL(*gl_, GenRenderbuffersEXT(_, _))
      .WillOnce(SetArgPointee<1>(kServiceRenderbufferId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenRenderbuffersImmediate>(client_renderbuffer_id_);
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(client_texture_id_);
  EXPECT_CALL(*gl_, GenBuffersARB(_, _))
      .WillOnce(SetArgPointee<1>(kServiceElementBufferId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenBuffersImmediate>(client_element_buffer_id_);
  GenHelper<cmds::GenQueriesEXTImmediate>(client_query_id_);

  DoCreateProgram(client_program_id_, kServiceProgramId);
  DoCreateShader(GL_VERTEX_SHADER, client_shader_id_, kServiceShaderId);

  if (init.context_type == CONTEXT_TYPE_WEBGL2 ||
      init.context_type == CONTEXT_TYPE_OPENGLES3) {
    EXPECT_CALL(*gl_, GenSamplers(_, _))
        .WillOnce(SetArgPointee<1>(kServiceSamplerId))
        .RetiresOnSaturation();
    GenHelper<cmds::GenSamplersImmediate>(client_sampler_id_);

    EXPECT_CALL(*gl_, GenTransformFeedbacks(_, _))
        .WillOnce(SetArgPointee<1>(kServiceTransformFeedbackId))
        .RetiresOnSaturation();
    GenHelper<cmds::GenTransformFeedbacksImmediate>(
        client_transformfeedback_id_);

    DoFenceSync(client_sync_id_, kServiceSyncId);
  }

  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  return result;
}

void GLES2DecoderTestBase::ResetDecoder() {
  if (decoder_.get()) {
    // All Tests should have read all their GLErrors before getting here.
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    if (!decoder_->WasContextLost()) {
      EXPECT_CALL(*gl_, DeleteBuffersARB(1, _)).Times(2).RetiresOnSaturation();
      EXPECT_CALL(*gl_, DeleteFramebuffersEXT(1, _)).Times(AnyNumber());
      if (group_->feature_info()->feature_flags().native_vertex_array_object) {
        EXPECT_CALL(*gl_,
                    DeleteVertexArraysOES(1, Pointee(kServiceVertexArrayId)))
            .Times(1)
            .RetiresOnSaturation();
      }
      if (group_->feature_info()->IsWebGL2OrES3Context()) {
        // fake default transform feedback.
        EXPECT_CALL(*gl_, DeleteTransformFeedbacks(1, _))
            .Times(1)
            .RetiresOnSaturation();
      }
      if (group_->feature_info()->IsWebGL2OrES3Context()) {
        // |client_transformfeedback_id_|
        EXPECT_CALL(*gl_, DeleteTransformFeedbacks(1, _))
            .Times(1)
            .RetiresOnSaturation();
      }
    }

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
  }
  gl::MockGLInterface::SetGLInterface(nullptr);
  gl_.reset();
  gl::GLSurfaceTestSupport::ShutdownGL(display_);
}

void GLES2DecoderTestBase::TearDown() {
  ResetDecoder();
}

GLint GLES2DecoderTestBase::GetGLError() {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::GetError cmd;
  cmd.Init(shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  return static_cast<GLint>(*GetSharedMemoryAs<GLenum*>());
}

void GLES2DecoderTestBase::DoCreateShader(
    GLenum shader_type, GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, CreateShader(shader_type))
      .Times(1)
      .WillOnce(Return(service_id))
      .RetiresOnSaturation();
  cmds::CreateShader cmd;
  cmd.Init(shader_type, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

bool GLES2DecoderTestBase::DoIsShader(GLuint client_id) {
  return IsObjectHelper<cmds::IsShader, cmds::IsShader::Result>(client_id);
}

void GLES2DecoderTestBase::DoDeleteShader(
    GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, DeleteShader(service_id))
      .Times(1)
      .RetiresOnSaturation();
  cmds::DeleteShader cmd;
  cmd.Init(client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoCreateProgram(
    GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, CreateProgram())
      .Times(1)
      .WillOnce(Return(service_id))
      .RetiresOnSaturation();
  cmds::CreateProgram cmd;
  cmd.Init(client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

bool GLES2DecoderTestBase::DoIsProgram(GLuint client_id) {
  return IsObjectHelper<cmds::IsProgram, cmds::IsProgram::Result>(client_id);
}

void GLES2DecoderTestBase::DoDeleteProgram(
    GLuint client_id, GLuint /* service_id */) {
  cmds::DeleteProgram cmd;
  cmd.Init(client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoFenceSync(
    GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0))
      .Times(1)
      .WillOnce(Return(reinterpret_cast<GLsync>(service_id)))
      .RetiresOnSaturation();
  cmds::FenceSync cmd;
  cmd.Init(client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoCreateSampler(
    GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, GenSamplers(1, _))
      .WillOnce(SetArgPointee<1>(service_id));
  cmds::GenSamplersImmediate* cmd =
      GetImmediateAs<cmds::GenSamplersImmediate>();
  GLuint temp = client_id;
  cmd->Init(1, &temp);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(*cmd, sizeof(temp)));
}

void GLES2DecoderTestBase::DoBindSampler(
    GLuint unit, GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, BindSampler(unit, service_id))
      .Times(1)
      .RetiresOnSaturation();
  cmds::BindSampler cmd;
  cmd.Init(unit, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoDeleteSampler(
    GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, DeleteSamplers(1, Pointee(service_id)))
      .Times(1)
      .RetiresOnSaturation();
  GenHelper<cmds::DeleteSamplersImmediate>(client_id);
}

void GLES2DecoderTestBase::DoCreateTransformFeedback(
    GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, GenTransformFeedbacks(1, _))
      .WillOnce(SetArgPointee<1>(service_id));
  cmds::GenTransformFeedbacksImmediate* cmd =
      GetImmediateAs<cmds::GenTransformFeedbacksImmediate>();
  GLuint temp = client_id;
  cmd->Init(1, &temp);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(*cmd, sizeof(temp)));
}

void GLES2DecoderTestBase::DoBindTransformFeedback(
    GLenum target, GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, BindTransformFeedback(target, service_id))
      .Times(1)
      .RetiresOnSaturation();
  cmds::BindTransformFeedback cmd;
  cmd.Init(target, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoDeleteTransformFeedback(
    GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, DeleteTransformFeedbacks(1, Pointee(service_id)))
      .Times(1)
      .RetiresOnSaturation();
  GenHelper<cmds::DeleteTransformFeedbacksImmediate>(client_id);
}

void GLES2DecoderTestBase::SetBucketData(
    uint32_t bucket_id, const void* data, uint32_t data_size) {
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

void GLES2DecoderTestBase::SetBucketAsCString(uint32_t bucket_id,
                                              const char* str) {
  SetBucketData(bucket_id, str, str ? (strlen(str) + 1) : 0);
}

void GLES2DecoderTestBase::SetBucketAsCStrings(uint32_t bucket_id,
                                               GLsizei count,
                                               const char** str,
                                               GLsizei count_in_header,
                                               char str_end) {
  uint32_t header_size = sizeof(GLint) * (count + 1);
  uint32_t total_size = header_size;
  auto header = base::HeapArray<GLint>::Uninit(count + 1);
  header[0] = static_cast<GLint>(count_in_header);
  for (GLsizei ii = 0; ii < count; ++ii) {
    header[ii + 1] = str && str[ii] ? strlen(str[ii]) : 0;
    total_size += header[ii + 1] + 1;
  }
  cmd::SetBucketSize cmd1;
  cmd1.Init(bucket_id, total_size);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
  memcpy(shared_memory_address_, header.data(), header_size);
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

void GLES2DecoderTestBase::SetupClearTextureExpectations(
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

void GLES2DecoderTestBase::SetupClearTexture3DExpectations(
    GLsizeiptr buffer_size,
    GLenum target,
    GLuint tex_service_id,
    GLint level,
    GLenum format,
    GLenum type,
    size_t tex_sub_image_3d_num_calls,
    GLint* xoffset,
    GLint* yoffset,
    GLint* zoffset,
    GLsizei* width,
    GLsizei* height,
    GLsizei* depth,
    GLuint bound_pixel_unpack_buffer) {
  InSequence seq;
  EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_ALIGNMENT, 1))
      .Times(1)
      .RetiresOnSaturation();
  if (bound_pixel_unpack_buffer) {
    EXPECT_CALL(*gl_, BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_ROW_LENGTH, 0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0))
        .Times(1)
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, GenBuffersARB(1, _)).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindBuffer(GL_PIXEL_UNPACK_BUFFER, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(
      *gl_, BufferData(GL_PIXEL_UNPACK_BUFFER, buffer_size, _, GL_STATIC_DRAW))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindTexture(target, tex_service_id))
      .Times(1)
      .RetiresOnSaturation();
  for (size_t ii = 0; ii < tex_sub_image_3d_num_calls; ++ii) {
    EXPECT_CALL(*gl_, TexSubImage3DNoData(target, level, xoffset[ii],
                                          yoffset[ii], zoffset[ii], width[ii],
                                          height[ii], depth[ii], format, type))
        .Times(1)
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, DeleteBuffersARB(1, _)).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_ALIGNMENT, _))
      .Times(1)
      .RetiresOnSaturation();
  if (bound_pixel_unpack_buffer) {
    EXPECT_CALL(*gl_,
                BindBuffer(GL_PIXEL_UNPACK_BUFFER, bound_pixel_unpack_buffer))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_ROW_LENGTH, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_IMAGE_HEIGHT, _))
        .Times(1)
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, BindTexture(target, _)).Times(1).RetiresOnSaturation();
}

void GLES2DecoderTestBase::SetupExpectationsForFramebufferClearing(
    GLenum target,
    GLuint clear_bits,
    GLclampf restore_red,
    GLclampf restore_green,
    GLclampf restore_blue,
    GLclampf restore_alpha,
    GLuint restore_stencil,
    GLclampf restore_depth,
    bool restore_scissor_test,
    GLint restore_scissor_x,
    GLint restore_scissor_y,
    GLsizei restore_scissor_width,
    GLsizei restore_scissor_height) {
  SetupExpectationsForFramebufferClearingMulti(
      0, 0, target, clear_bits, restore_red, restore_green, restore_blue,
      restore_alpha, restore_stencil, restore_depth, restore_scissor_test,
      restore_scissor_x, restore_scissor_y, restore_scissor_width,
      restore_scissor_height);
}

void GLES2DecoderTestBase::SetupExpectationsForRestoreClearState(
    GLclampf restore_red,
    GLclampf restore_green,
    GLclampf restore_blue,
    GLclampf restore_alpha,
    GLuint restore_stencil,
    GLclampf restore_depth,
    bool restore_scissor_test,
    GLint restore_scissor_x,
    GLint restore_scissor_y,
    GLsizei restore_scissor_width,
    GLsizei restore_scissor_height) {
  EXPECT_CALL(*gl_, ClearColor(
      restore_red, restore_green, restore_blue, restore_alpha))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ClearStencil(restore_stencil))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ClearDepth(restore_depth))
      .Times(1)
      .RetiresOnSaturation();
  SetupExpectationsForEnableDisable(GL_SCISSOR_TEST, restore_scissor_test);
  if (group_->feature_info()->feature_flags().ext_window_rectangles) {
    EXPECT_CALL(*gl_, WindowRectanglesEXT(_, _, _))
        .Times(1)
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, Scissor(restore_scissor_x, restore_scissor_y,
                            restore_scissor_width, restore_scissor_height))
      .Times(1)
      .RetiresOnSaturation();
}

void GLES2DecoderTestBase::SetupExpectationsForFramebufferClearingMulti(
    GLuint read_framebuffer_service_id,
    GLuint draw_framebuffer_service_id,
    GLenum target,
    GLuint clear_bits,
    GLclampf restore_red,
    GLclampf restore_green,
    GLclampf restore_blue,
    GLclampf restore_alpha,
    GLuint restore_stencil,
    GLclampf restore_depth,
    bool restore_scissor_test,
    GLint restore_scissor_x,
    GLint restore_scissor_y,
    GLsizei restore_scissor_width,
    GLsizei restore_scissor_height) {
  // TODO(gman): Figure out why InSequence stopped working.
  // InSequence sequence;
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(target))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  if (target == GL_READ_FRAMEBUFFER_EXT) {
    EXPECT_CALL(*gl_, BindFramebufferEXT(
        GL_DRAW_FRAMEBUFFER_EXT, read_framebuffer_service_id))
        .Times(1)
        .RetiresOnSaturation();
  }
  if ((clear_bits & GL_COLOR_BUFFER_BIT) != 0) {
    EXPECT_CALL(*gl_, ClearColor(0.0f, 0.0f, 0.0f, 0.0f))
        .Times(1)
        .RetiresOnSaturation();
    SetupExpectationsForColorMask(true, true, true, true);
  }
  if ((clear_bits & GL_STENCIL_BUFFER_BIT) != 0) {
    EXPECT_CALL(*gl_, ClearStencil(0))
        .Times(1)
        .RetiresOnSaturation();
    SetupExpectationsForStencilMask(static_cast<GLuint>(-1),
                                    static_cast<GLuint>(-1));
  }
  if ((clear_bits & GL_DEPTH_BUFFER_BIT) != 0) {
    EXPECT_CALL(*gl_, ClearDepth(1.0f))
        .Times(1)
        .RetiresOnSaturation();
    SetupExpectationsForDepthMask(true);
  }
  SetupExpectationsForEnableDisable(GL_SCISSOR_TEST, false);
  if (group_->feature_info()->feature_flags().ext_window_rectangles) {
    EXPECT_CALL(*gl_, WindowRectanglesEXT(GL_EXCLUSIVE_EXT, 0, nullptr))
        .Times(1)
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, Clear(clear_bits))
      .Times(1)
      .RetiresOnSaturation();
  SetupExpectationsForRestoreClearState(
      restore_red, restore_green, restore_blue, restore_alpha, restore_stencil,
      restore_depth, restore_scissor_test, restore_scissor_x, restore_scissor_y,
      restore_scissor_width, restore_scissor_height);
  if (target == GL_READ_FRAMEBUFFER_EXT) {
    EXPECT_CALL(*gl_, BindFramebufferEXT(
        GL_DRAW_FRAMEBUFFER_EXT, draw_framebuffer_service_id))
        .Times(1)
        .RetiresOnSaturation();
  }
}

void GLES2DecoderTestBase::SetupShaderForUniform(GLenum uniform_type) {
  static AttribInfo attribs[] = {
    { "foo", 1, GL_FLOAT, 1, },
    { "goo", 1, GL_FLOAT, 2, },
  };
  UniformInfo uniforms[] = {
    { "bar", 1, uniform_type, 0, 2, -1, },
    { "car", 4, uniform_type, 1, 1, -1, },
  };
  const GLuint kTestClientVertexShaderId = 5001;
  const GLuint kTestServiceVertexShaderId = 6001;
  const GLuint kTestClientFragmentShaderId = 5002;
  const GLuint kTestServiceFragmentShaderId = 6002;
  SetupShader(attribs, std::size(attribs), uniforms, std::size(uniforms),
              client_program_id_, kServiceProgramId, kTestClientVertexShaderId,
              kTestServiceVertexShaderId, kTestClientFragmentShaderId,
              kTestServiceFragmentShaderId);

  EXPECT_CALL(*gl_, UseProgram(kServiceProgramId))
      .Times(1)
      .RetiresOnSaturation();
  cmds::UseProgram cmd;
  cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoBindBuffer(
    GLenum target, GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, BindBuffer(target, service_id))
      .Times(1)
      .RetiresOnSaturation();
  if (target == GL_PIXEL_PACK_BUFFER) {
    EXPECT_CALL(*gl_, PixelStorei(GL_PACK_ROW_LENGTH, _))
        .Times(1)
        .RetiresOnSaturation();
  }
  cmds::BindBuffer cmd;
  cmd.Init(target, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

bool GLES2DecoderTestBase::DoIsBuffer(GLuint client_id) {
  return IsObjectHelper<cmds::IsBuffer, cmds::IsBuffer::Result>(client_id);
}

void GLES2DecoderTestBase::DoDeleteBuffer(
    GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, DeleteBuffersARB(1, Pointee(service_id)))
      .Times(1)
      .RetiresOnSaturation();
  GenHelper<cmds::DeleteBuffersImmediate>(client_id);
}

void GLES2DecoderTestBase::SetupExpectationsForColorMask(bool red,
                                                         bool green,
                                                         bool blue,
                                                         bool alpha) {
  if (ignore_cached_state_for_test_ || cached_color_mask_red_ != red ||
      cached_color_mask_green_ != green || cached_color_mask_blue_ != blue ||
      cached_color_mask_alpha_ != alpha) {
    cached_color_mask_red_ = red;
    cached_color_mask_green_ = green;
    cached_color_mask_blue_ = blue;
    cached_color_mask_alpha_ = alpha;
    EXPECT_CALL(*gl_, ColorMask(red, green, blue, alpha))
        .Times(1)
        .RetiresOnSaturation();
  }
}

void GLES2DecoderTestBase::SetupExpectationsForDepthMask(bool mask) {
  if (ignore_cached_state_for_test_ || cached_depth_mask_ != mask) {
    cached_depth_mask_ = mask;
    EXPECT_CALL(*gl_, DepthMask(mask)).Times(1).RetiresOnSaturation();
  }
}

void GLES2DecoderTestBase::SetupExpectationsForStencilMask(GLuint front_mask,
                                                           GLuint back_mask) {
  if (ignore_cached_state_for_test_ ||
      cached_stencil_front_mask_ != front_mask) {
    cached_stencil_front_mask_ = front_mask;
    EXPECT_CALL(*gl_, StencilMaskSeparate(GL_FRONT, front_mask))
        .Times(1)
        .RetiresOnSaturation();
  }

  if (ignore_cached_state_for_test_ ||
      cached_stencil_back_mask_ != back_mask) {
    cached_stencil_back_mask_ = back_mask;
    EXPECT_CALL(*gl_, StencilMaskSeparate(GL_BACK, back_mask))
        .Times(1)
        .RetiresOnSaturation();
  }
}

void GLES2DecoderTestBase::SetupExpectationsForEnableDisable(GLenum cap,
                                                             bool enable) {
  switch (cap) {
    case GL_BLEND:
      if (enable_flags_.cached_blend == enable &&
          !ignore_cached_state_for_test_)
        return;
      enable_flags_.cached_blend = enable;
      break;
    case GL_CULL_FACE:
      if (enable_flags_.cached_cull_face == enable &&
          !ignore_cached_state_for_test_)
        return;
      enable_flags_.cached_cull_face = enable;
      break;
    case GL_DEPTH_TEST:
      if (enable_flags_.cached_depth_test == enable &&
          !ignore_cached_state_for_test_)
        return;
      enable_flags_.cached_depth_test = enable;
      break;
    case GL_DITHER:
      if (enable_flags_.cached_dither == enable &&
          !ignore_cached_state_for_test_)
        return;
      enable_flags_.cached_dither = enable;
      break;
    case GL_POLYGON_OFFSET_FILL:
      if (enable_flags_.cached_polygon_offset_fill == enable &&
          !ignore_cached_state_for_test_)
        return;
      enable_flags_.cached_polygon_offset_fill = enable;
      break;
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
      if (enable_flags_.cached_sample_alpha_to_coverage == enable &&
          !ignore_cached_state_for_test_)
        return;
      enable_flags_.cached_sample_alpha_to_coverage = enable;
      break;
    case GL_SAMPLE_COVERAGE:
      if (enable_flags_.cached_sample_coverage == enable &&
          !ignore_cached_state_for_test_)
        return;
      enable_flags_.cached_sample_coverage = enable;
      break;
    case GL_SCISSOR_TEST:
      if (enable_flags_.cached_scissor_test == enable &&
          !ignore_cached_state_for_test_)
        return;
      enable_flags_.cached_scissor_test = enable;
      break;
    case GL_STENCIL_TEST:
      if (enable_flags_.cached_stencil_test == enable &&
          !ignore_cached_state_for_test_)
        return;
      enable_flags_.cached_stencil_test = enable;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  if (enable) {
    EXPECT_CALL(*gl_, Enable(cap)).Times(1).RetiresOnSaturation();
  } else {
    EXPECT_CALL(*gl_, Disable(cap)).Times(1).RetiresOnSaturation();
  }
}

void GLES2DecoderTestBase::SetupExpectationsForApplyingDirtyState(
    bool framebuffer_is_rgb,
    bool framebuffer_has_depth,
    bool framebuffer_has_stencil,
    GLuint color_bits,
    bool depth_mask,
    bool depth_enabled,
    GLuint front_stencil_mask,
    GLuint back_stencil_mask,
    bool stencil_enabled) {
  bool color_mask_red = (color_bits & 0x1000) != 0;
  bool color_mask_green = (color_bits & 0x0100) != 0;
  bool color_mask_blue = (color_bits & 0x0010) != 0;
  bool color_mask_alpha = (color_bits & 0x0001) && !framebuffer_is_rgb;

  SetupExpectationsForColorMask(
      color_mask_red, color_mask_green, color_mask_blue, color_mask_alpha);
  SetupExpectationsForDepthMask(depth_mask);
  SetupExpectationsForStencilMask(front_stencil_mask, back_stencil_mask);
  SetupExpectationsForEnableDisable(GL_DEPTH_TEST,
                                    framebuffer_has_depth && depth_enabled);
  SetupExpectationsForEnableDisable(GL_STENCIL_TEST,
                                    framebuffer_has_stencil && stencil_enabled);
}

void GLES2DecoderTestBase::SetupExpectationsForApplyingDefaultDirtyState() {
  SetupExpectationsForApplyingDirtyState(false,   // Framebuffer is RGB
                                         false,   // Framebuffer has depth
                                         false,   // Framebuffer has stencil
                                         0x1111,  // color bits
                                         true,    // depth mask
                                         false,   // depth enabled
                                         0,       // front stencil mask
                                         0,       // back stencil mask
                                         false);  // stencil enabled
}

GLES2DecoderTestBase::EnableFlags::EnableFlags()
    : cached_blend(false),
      cached_cull_face(false),
      cached_depth_test(false),
      cached_dither(true),
      cached_polygon_offset_fill(false),
      cached_sample_alpha_to_coverage(false),
      cached_sample_coverage(false),
      cached_scissor_test(false),
      cached_stencil_test(false) {
}

void GLES2DecoderTestBase::DoBindFramebuffer(
    GLenum target, GLuint client_id, GLuint service_id) {
  if (group_->feature_info()->feature_flags().ext_window_rectangles) {
    EXPECT_CALL(*gl_, WindowRectanglesEXT(_, _, _))
        .Times(::testing::AtMost(1))
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, BindFramebufferEXT(target, service_id))
      .Times(1)
      .RetiresOnSaturation();
  cmds::BindFramebuffer cmd;
  cmd.Init(target, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

bool GLES2DecoderTestBase::DoIsFramebuffer(GLuint client_id) {
  return IsObjectHelper<cmds::IsFramebuffer, cmds::IsFramebuffer::Result>(
      client_id);
}

void GLES2DecoderTestBase::DoDeleteFramebuffer(
    GLuint client_id, GLuint service_id,
    bool reset_draw, GLenum draw_target, GLuint draw_id,
    bool reset_read, GLenum read_target, GLuint read_id) {
  if (reset_draw) {
    EXPECT_CALL(*gl_, BindFramebufferEXT(draw_target, draw_id))
        .Times(1)
        .RetiresOnSaturation();
  }
  if (reset_read) {
    EXPECT_CALL(*gl_, BindFramebufferEXT(read_target, read_id))
        .Times(1)
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, DeleteFramebuffersEXT(1, Pointee(service_id)))
      .Times(1)
      .RetiresOnSaturation();
  GenHelper<cmds::DeleteFramebuffersImmediate>(client_id);
}

void GLES2DecoderTestBase::DoBindRenderbuffer(
    GLenum target, GLuint client_id, GLuint service_id) {
  service_renderbuffer_id_ = service_id;
  service_renderbuffer_valid_ = true;
  EXPECT_CALL(*gl_, BindRenderbufferEXT(target, service_id))
      .Times(1)
      .RetiresOnSaturation();
  cmds::BindRenderbuffer cmd;
  cmd.Init(target, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::SetupExpectationsForInternalFormatSampleCountsHelper(
    GLenum target,
    GLenum internal_format,
    GLint expected_num_sample_counts,
    GLint expected_sample0) {
  EXPECT_CALL(*gl_, GetInternalformativ(target, internal_format,
                                        GL_NUM_SAMPLE_COUNTS, 1, _))
      .WillOnce(SetArgPointee<4>(expected_num_sample_counts))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetInternalformativ(target, internal_format, GL_SAMPLES,
                                        expected_num_sample_counts, _))
      .WillOnce(SetArgPointee<4>(expected_sample0))
      .RetiresOnSaturation();
}

void GLES2DecoderTestBase::DoRenderbufferStorageMultisampleCHROMIUM(
    GLenum target,
    GLsizei samples,
    GLenum internal_format,
    GLsizei width,
    GLsizei height,
    bool expect_bind) {
  SetupExpectationsForInternalFormatSampleCountsHelper(target, internal_format,
                                                       1, samples);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EnsureRenderbufferBound(expect_bind);
  EXPECT_CALL(*gl_, RenderbufferStorageMultisample(
                        target, samples, internal_format, width, height))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::RenderbufferStorageMultisampleCHROMIUM cmd;
  cmd.Init(target, samples, internal_format, width, height);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

void GLES2DecoderTestBase::RestoreRenderbufferBindings() {
  GetDecoder()->RestoreRenderbufferBindings();
  service_renderbuffer_valid_ = false;
}

void GLES2DecoderTestBase::EnsureRenderbufferBound(bool expect_bind) {
  EXPECT_NE(expect_bind, service_renderbuffer_valid_);

  if (expect_bind) {
    service_renderbuffer_valid_ = true;
    EXPECT_CALL(*gl_,
                BindRenderbufferEXT(GL_RENDERBUFFER, service_renderbuffer_id_))
        .Times(1)
        .RetiresOnSaturation();
  } else {
    EXPECT_CALL(*gl_, BindRenderbufferEXT(_, _)).Times(0);
  }
}

bool GLES2DecoderTestBase::DoIsRenderbuffer(GLuint client_id) {
  return IsObjectHelper<cmds::IsRenderbuffer, cmds::IsRenderbuffer::Result>(
      client_id);
}

void GLES2DecoderTestBase::DoDeleteRenderbuffer(
    GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, DeleteRenderbuffersEXT(1, Pointee(service_id)))
      .Times(1)
      .RetiresOnSaturation();
  GenHelper<cmds::DeleteRenderbuffersImmediate>(client_id);
}

void GLES2DecoderTestBase::DoBindTexture(
    GLenum target, GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, BindTexture(target, service_id))
      .Times(1)
      .RetiresOnSaturation();
  cmds::BindTexture cmd;
  cmd.Init(target, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

bool GLES2DecoderTestBase::DoIsTexture(GLuint client_id) {
  return IsObjectHelper<cmds::IsTexture, cmds::IsTexture::Result>(client_id);
}

void GLES2DecoderTestBase::DoDeleteTexture(
    GLuint client_id, GLuint service_id) {

  {
    InSequence s;

    // Calling DoDeleteTexture will unbind the texture from any texture units
    // it's currently bound to.
    EXPECT_CALL(*gl_, BindTexture(_, 0))
      .Times(AnyNumber());

    EXPECT_CALL(*gl_, DeleteTextures(1, Pointee(service_id)))
        .Times(1)
        .RetiresOnSaturation();

    GenHelper<cmds::DeleteTexturesImmediate>(client_id);
  }
}

void GLES2DecoderTestBase::DoTexImage2D(GLenum target,
                                        GLint level,
                                        GLenum internal_format,
                                        GLsizei width,
                                        GLsizei height,
                                        GLint border,
                                        GLenum format,
                                        GLenum type,
                                        uint32_t shared_memory_id,
                                        uint32_t shared_memory_offset) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  bool emulated_format = group_->feature_info()->gl_version_info().is_es3 &&
                         (format == GL_LUMINANCE ||
                          format == GL_LUMINANCE_ALPHA || format == GL_ALPHA);
  if (emulated_format) {
    // The format of these textures may be different than requested due to
    // emulation.
    EXPECT_CALL(*gl_,
                TexImage2D(target, level, _, width, height, border, _, type, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, TexParameteri(target, GL_TEXTURE_SWIZZLE_R, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, TexParameteri(target, GL_TEXTURE_SWIZZLE_G, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, TexParameteri(target, GL_TEXTURE_SWIZZLE_B, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, TexParameteri(target, GL_TEXTURE_SWIZZLE_A, _))
        .Times(1)
        .RetiresOnSaturation();
  } else {
    EXPECT_CALL(*gl_, TexImage2D(target, level, internal_format, width, height,
                                 border, format, type, _))
        .Times(1)
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::TexImage2D cmd;
  cmd.Init(target, level, internal_format, width, height, format,
           type, shared_memory_id, shared_memory_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoTexImage2DConvertInternalFormat(
    GLenum target,
    GLint level,
    GLenum requested_internal_format,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    uint32_t shared_memory_id,
    uint32_t shared_memory_offset,
    GLenum expected_internal_format) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, TexImage2D(target, level, expected_internal_format,
                               width, height, border, format, type, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::TexImage2D cmd;
  cmd.Init(target, level, requested_internal_format, width, height,
           format, type, shared_memory_id, shared_memory_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoCompressedTexImage2D(GLenum target,
                                                  GLint level,
                                                  GLenum format,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLint border,
                                                  GLsizei size,
                                                  uint32_t bucket_id) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, CompressedTexImage2D(
      target, level, format, width, height, border, size, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  CommonDecoder::Bucket* bucket = decoder_->CreateBucket(bucket_id);
  bucket->SetSize(size);
  cmds::CompressedTexImage2DBucket cmd;
  cmd.Init(
      target, level, format, width, height,
      bucket_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoTexImage3D(GLenum target,
                                        GLint level,
                                        GLenum internal_format,
                                        GLsizei width,
                                        GLsizei height,
                                        GLsizei depth,
                                        GLint border,
                                        GLenum format,
                                        GLenum type,
                                        uint32_t shared_memory_id,
                                        uint32_t shared_memory_offset) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, TexImage3D(target, level, internal_format,
                               width, height, depth, border, format, type, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::TexImage3D cmd;
  cmd.Init(target, level, internal_format, width, height, depth, format, type,
           shared_memory_id, shared_memory_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoCopyTexImage2D(
    GLenum target,
    GLint level,
    GLenum internal_format,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLint border) {
  GLenum translated_internal_format = internal_format;
  if (group_->feature_info()->IsWebGL2OrES3Context()) {
    if (internal_format == GL_RGB) {
      translated_internal_format = GL_RGB8;
    } else if (internal_format == GL_RGBA) {
      translated_internal_format = GL_RGBA8;
    }
  }
  // For GL_BGRA_EXT, we have to fall back to TexImage2D and
  // CopyTexSubImage2D, since GL_BGRA_EXT is not accepted by CopyTexImage2D.
  // In some cases this fallback further triggers set and restore of
  // GL_UNPACK_ALIGNMENT.
  if (internal_format == GL_BGRA_EXT) {
    EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_ALIGNMENT, _))
        .Times(2)
        .RetiresOnSaturation();

    EXPECT_CALL(*gl_, TexImage2D(target, level, internal_format,
                                 width, height, border,
                                 internal_format, GL_UNSIGNED_BYTE, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, CopyTexSubImage2D(target, level, 0, 0, 0, 0,
                                        width, height))
        .Times(1)
        .RetiresOnSaturation();
  } else if (group_->feature_info()->gl_version_info().is_es3) {
    bool emulated = internal_format == GL_ALPHA ||
                    internal_format == GL_LUMINANCE ||
                    internal_format == GL_LUMINANCE_ALPHA;
    if (emulated) {
      EXPECT_CALL(*gl_, TexParameteri(target, GL_TEXTURE_SWIZZLE_R, _))
          .Times(testing::AtLeast(1));
      EXPECT_CALL(*gl_, TexParameteri(target, GL_TEXTURE_SWIZZLE_G, _))
          .Times(testing::AtLeast(1));
      EXPECT_CALL(*gl_, TexParameteri(target, GL_TEXTURE_SWIZZLE_B, _))
          .Times(testing::AtLeast(1));
      EXPECT_CALL(*gl_, TexParameteri(target, GL_TEXTURE_SWIZZLE_A, _))
          .Times(testing::AtLeast(1));
    } else {
      EXPECT_CALL(
          *gl_, CopyTexImage2D(target, level, translated_internal_format, 0, 0,
                               width, height, border))
          .Times(1)
          .RetiresOnSaturation();
    }
  } else {
    EXPECT_CALL(*gl_, CopyTexImage2D(target, level, translated_internal_format,
                                     0, 0, width, height, border))
        .Times(1)
        .RetiresOnSaturation();
  }
  cmds::CopyTexImage2D cmd;
  cmd.Init(target, level, internal_format, 0, 0, width, height);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoRenderbufferStorage(GLenum target,
                                                 GLenum internal_format,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLenum error) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              RenderbufferStorageEXT(target, internal_format, width, height))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(error))
      .RetiresOnSaturation();
  cmds::RenderbufferStorage cmd;
  cmd.Init(target, internal_format, width, height);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoFramebufferTexture2D(
    GLenum target, GLenum attachment, GLenum textarget,
    GLuint texture_client_id, GLuint texture_service_id, GLint level,
    GLenum error) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(
      target, attachment, textarget, texture_service_id, level))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(error))
      .RetiresOnSaturation();
  cmds::FramebufferTexture2D cmd;
  cmd.Init(target, attachment, textarget, texture_client_id, level);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoFramebufferRenderbuffer(
    GLenum target,
    GLenum attachment,
    GLenum renderbuffer_target,
    GLuint renderbuffer_client_id,
    GLuint renderbuffer_service_id,
    GLenum error) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
    EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(
        target, GL_DEPTH_ATTACHMENT, renderbuffer_target,
        renderbuffer_service_id))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(error))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(
        target, GL_STENCIL_ATTACHMENT, renderbuffer_target,
        renderbuffer_service_id))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(error))
        .RetiresOnSaturation();
  } else {
    EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(
        target, attachment, renderbuffer_target, renderbuffer_service_id))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(error))
        .RetiresOnSaturation();
  }
  cmds::FramebufferRenderbuffer cmd;
  cmd.Init(target, attachment, renderbuffer_target, renderbuffer_client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

GLenum GLES2DecoderTestBase::DoCheckFramebufferStatus(GLenum target) {
  auto* result = static_cast<cmds::CheckFramebufferStatus::Result*>(
      shared_memory_address_);
  *result = 0;
  cmds::CheckFramebufferStatus cmd;
  cmd.Init(GL_FRAMEBUFFER, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  return *result;
}

void GLES2DecoderTestBase::DoVertexAttribPointer(
    GLuint index, GLint size, GLenum type, GLsizei stride, GLuint offset) {
  EXPECT_CALL(*gl_,
              VertexAttribPointer(index, size, type, GL_FALSE, stride,
                                  BufferOffset(offset)))
      .Times(1)
      .RetiresOnSaturation();
  cmds::VertexAttribPointer cmd;
  cmd.Init(index, size, GL_FLOAT, GL_FALSE, stride, offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoVertexAttribDivisorANGLE(
    GLuint index, GLuint divisor) {
  EXPECT_CALL(*gl_,
              VertexAttribDivisorANGLE(index, divisor))
      .Times(1)
      .RetiresOnSaturation();
  cmds::VertexAttribDivisorANGLE cmd;
  cmd.Init(index, divisor);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::AddExpectationsForGenVertexArraysOES() {
  if (group_->feature_info()->feature_flags().native_vertex_array_object) {
      EXPECT_CALL(*gl_, GenVertexArraysOES(1, _))
          .WillOnce(SetArgPointee<1>(kServiceVertexArrayId))
          .RetiresOnSaturation();
  }
}

void GLES2DecoderTestBase::AddExpectationsForDeleteVertexArraysOES() {
  if (group_->feature_info()->feature_flags().native_vertex_array_object) {
      EXPECT_CALL(*gl_, DeleteVertexArraysOES(1, _))
          .Times(1)
          .RetiresOnSaturation();
  }
}

void GLES2DecoderTestBase::AddExpectationsForDeleteBoundVertexArraysOES() {
  // Expectations are the same as a delete, followed by binding VAO 0.
  AddExpectationsForDeleteVertexArraysOES();
  AddExpectationsForBindVertexArrayOES();
}

void GLES2DecoderTestBase::AddExpectationsForBindVertexArrayOES() {
  if (group_->feature_info()->feature_flags().native_vertex_array_object) {
    EXPECT_CALL(*gl_, BindVertexArrayOES(_))
      .Times(1)
      .RetiresOnSaturation();
  } else {
    for (uint32_t vv = 0; vv < group_->max_vertex_attribs(); ++vv) {
      AddExpectationsForRestoreAttribState(vv);
    }

    EXPECT_CALL(*gl_, BindBuffer(GL_ELEMENT_ARRAY_BUFFER, _))
      .Times(1)
      .RetiresOnSaturation();
  }
}

void GLES2DecoderTestBase::AddExpectationsForRestoreAttribState(GLuint attrib) {
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

  // TODO(bajones): Not sure if I can tell which of these will be called
  EXPECT_CALL(*gl_, EnableVertexAttribArray(attrib))
      .Times(testing::AtMost(1))
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, DisableVertexAttribArray(attrib))
      .Times(testing::AtMost(1))
      .RetiresOnSaturation();
}

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const int GLES2DecoderTestBase::kBackBufferWidth;
const int GLES2DecoderTestBase::kBackBufferHeight;

const GLint GLES2DecoderTestBase::kMaxTextureSize;
const GLint GLES2DecoderTestBase::kMaxCubeMapTextureSize;
const GLint GLES2DecoderTestBase::kNumVertexAttribs;
const GLint GLES2DecoderTestBase::kNumTextureUnits;
const GLint GLES2DecoderTestBase::kMaxTextureImageUnits;
const GLint GLES2DecoderTestBase::kMaxVertexTextureImageUnits;
const GLint GLES2DecoderTestBase::kMaxFragmentUniformVectors;
const GLint GLES2DecoderTestBase::kMaxVaryingVectors;
const GLint GLES2DecoderTestBase::kMaxVertexUniformVectors;
const GLint GLES2DecoderTestBase::kMaxViewportWidth;
const GLint GLES2DecoderTestBase::kMaxViewportHeight;

const GLuint GLES2DecoderTestBase::kServiceAttrib0BufferId;
const GLuint GLES2DecoderTestBase::kServiceFixedAttribBufferId;

const GLuint GLES2DecoderTestBase::kServiceBufferId;
const GLuint GLES2DecoderTestBase::kServiceFramebufferId;
const GLuint GLES2DecoderTestBase::kServiceRenderbufferId;
const GLuint GLES2DecoderTestBase::kServiceSamplerId;
const GLuint GLES2DecoderTestBase::kServiceTextureId;
const GLuint GLES2DecoderTestBase::kServiceProgramId;
const GLuint GLES2DecoderTestBase::kServiceShaderId;
const GLuint GLES2DecoderTestBase::kServiceElementBufferId;
const GLuint GLES2DecoderTestBase::kServiceQueryId;
const GLuint GLES2DecoderTestBase::kServiceVertexArrayId;
const GLuint GLES2DecoderTestBase::kServiceTransformFeedbackId;
const GLuint GLES2DecoderTestBase::kServiceDefaultTransformFeedbackId;
const GLuint GLES2DecoderTestBase::kServiceSyncId;

const size_t GLES2DecoderTestBase::kSharedBufferSize;
const uint32_t GLES2DecoderTestBase::kSharedMemoryOffset;
const int32_t GLES2DecoderTestBase::kInvalidSharedMemoryId;
const uint32_t GLES2DecoderTestBase::kInvalidSharedMemoryOffset;
const uint32_t GLES2DecoderTestBase::kInitialResult;
const uint8_t GLES2DecoderTestBase::kInitialMemoryValue;

const uint32_t GLES2DecoderTestBase::kNewClientId;
const uint32_t GLES2DecoderTestBase::kNewServiceId;
const uint32_t GLES2DecoderTestBase::kInvalidClientId;

const GLuint GLES2DecoderTestBase::kServiceVertexShaderId;
const GLuint GLES2DecoderTestBase::kServiceFragmentShaderId;

const GLuint GLES2DecoderTestBase::kServiceCopyTextureChromiumShaderId;
const GLuint GLES2DecoderTestBase::kServiceCopyTextureChromiumProgramId;

const GLuint GLES2DecoderTestBase::kServiceCopyTextureChromiumTextureBufferId;
const GLuint GLES2DecoderTestBase::kServiceCopyTextureChromiumVertexBufferId;
const GLuint GLES2DecoderTestBase::kServiceCopyTextureChromiumFBOId;
const GLuint GLES2DecoderTestBase::kServiceCopyTextureChromiumPositionAttrib;
const GLuint GLES2DecoderTestBase::kServiceCopyTextureChromiumTexAttrib;
const GLuint GLES2DecoderTestBase::kServiceCopyTextureChromiumSamplerLocation;

const GLsizei GLES2DecoderTestBase::kNumVertices;
const GLsizei GLES2DecoderTestBase::kNumIndices;
const int GLES2DecoderTestBase::kValidIndexRangeStart;
const int GLES2DecoderTestBase::kValidIndexRangeCount;
const int GLES2DecoderTestBase::kInvalidIndexRangeStart;
const int GLES2DecoderTestBase::kInvalidIndexRangeCount;
const int GLES2DecoderTestBase::kOutOfRangeIndexRangeEnd;
const GLuint GLES2DecoderTestBase::kMaxValidIndex;

const GLint GLES2DecoderTestBase::kMaxAttribLength;
const GLint GLES2DecoderTestBase::kAttrib1Size;
const GLint GLES2DecoderTestBase::kAttrib2Size;
const GLint GLES2DecoderTestBase::kAttrib3Size;
const GLint GLES2DecoderTestBase::kAttrib1Location;
const GLint GLES2DecoderTestBase::kAttrib2Location;
const GLint GLES2DecoderTestBase::kAttrib3Location;
const GLenum GLES2DecoderTestBase::kAttrib1Type;
const GLenum GLES2DecoderTestBase::kAttrib2Type;
const GLenum GLES2DecoderTestBase::kAttrib3Type;
const GLint GLES2DecoderTestBase::kInvalidAttribLocation;
const GLint GLES2DecoderTestBase::kBadAttribIndex;

const GLint GLES2DecoderTestBase::kMaxUniformLength;
const GLint GLES2DecoderTestBase::kUniform1Size;
const GLint GLES2DecoderTestBase::kUniform2Size;
const GLint GLES2DecoderTestBase::kUniform3Size;
const GLint GLES2DecoderTestBase::kUniform4Size;
const GLint GLES2DecoderTestBase::kUniform5Size;
const GLint GLES2DecoderTestBase::kUniform6Size;
const GLint GLES2DecoderTestBase::kUniform7Size;
const GLint GLES2DecoderTestBase::kUniform8Size;
const GLint GLES2DecoderTestBase::kUniform1RealLocation;
const GLint GLES2DecoderTestBase::kUniform2RealLocation;
const GLint GLES2DecoderTestBase::kUniform2ElementRealLocation;
const GLint GLES2DecoderTestBase::kUniform3RealLocation;
const GLint GLES2DecoderTestBase::kUniform4RealLocation;
const GLint GLES2DecoderTestBase::kUniform5RealLocation;
const GLint GLES2DecoderTestBase::kUniform6RealLocation;
const GLint GLES2DecoderTestBase::kUniform7RealLocation;
const GLint GLES2DecoderTestBase::kUniform8RealLocation;
const GLint GLES2DecoderTestBase::kUniform1FakeLocation;
const GLint GLES2DecoderTestBase::kUniform2FakeLocation;
const GLint GLES2DecoderTestBase::kUniform2ElementFakeLocation;
const GLint GLES2DecoderTestBase::kUniform3FakeLocation;
const GLint GLES2DecoderTestBase::kUniform4FakeLocation;
const GLint GLES2DecoderTestBase::kUniform5FakeLocation;
const GLint GLES2DecoderTestBase::kUniform6FakeLocation;
const GLint GLES2DecoderTestBase::kUniform7FakeLocation;
const GLint GLES2DecoderTestBase::kUniform8FakeLocation;
const GLint GLES2DecoderTestBase::kUniform1DesiredLocation;
const GLint GLES2DecoderTestBase::kUniform2DesiredLocation;
const GLint GLES2DecoderTestBase::kUniform3DesiredLocation;
const GLint GLES2DecoderTestBase::kUniform4DesiredLocation;
const GLint GLES2DecoderTestBase::kUniform5DesiredLocation;
const GLint GLES2DecoderTestBase::kUniform6DesiredLocation;
const GLint GLES2DecoderTestBase::kUniform7DesiredLocation;
const GLint GLES2DecoderTestBase::kUniform8DesiredLocation;
const GLenum GLES2DecoderTestBase::kUniform1Type;
const GLenum GLES2DecoderTestBase::kUniform2Type;
const GLenum GLES2DecoderTestBase::kUniform3Type;
const GLenum GLES2DecoderTestBase::kUniform4Type;
const GLenum GLES2DecoderTestBase::kUniform5Type;
const GLenum GLES2DecoderTestBase::kUniform6Type;
const GLenum GLES2DecoderTestBase::kUniform7Type;
const GLenum GLES2DecoderTestBase::kUniform8Type;
const GLenum GLES2DecoderTestBase::kUniformCubemapType;
const GLint GLES2DecoderTestBase::kInvalidUniformLocation;
const GLint GLES2DecoderTestBase::kBadUniformIndex;
const GLint GLES2DecoderTestBase::kOutputVariable1Size;
const GLenum GLES2DecoderTestBase::kOutputVariable1Type;
const GLuint GLES2DecoderTestBase::kOutputVariable1ColorName;
const GLuint GLES2DecoderTestBase::kOutputVariable1Index;
#endif

const char* GLES2DecoderTestBase::kAttrib1Name = "attrib1";
const char* GLES2DecoderTestBase::kAttrib2Name = "attrib2";
const char* GLES2DecoderTestBase::kAttrib3Name = "attrib3";
const char* GLES2DecoderTestBase::kUniform1Name = "uniform1";
const char* GLES2DecoderTestBase::kUniform2Name = "uniform2[0]";
const char* GLES2DecoderTestBase::kUniform3Name = "uniform3[0]";
const char* GLES2DecoderTestBase::kUniform4Name = "uniform4";
const char* GLES2DecoderTestBase::kUniform5Name = "uniform5";
const char* GLES2DecoderTestBase::kUniform6Name = "uniform6";
const char* GLES2DecoderTestBase::kUniform7Name = "uniform7";
const char* GLES2DecoderTestBase::kUniform8Name = "uniform8";

const char* GLES2DecoderTestBase::kOutputVariable1Name = "gl_FragColor";
const char* GLES2DecoderTestBase::kOutputVariable1NameESSL3 = "color";

void GLES2DecoderTestBase::SetupDefaultProgram() {
  {
    static AttribInfo attribs[] = {
      { kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location, },
      { kAttrib2Name, kAttrib2Size, kAttrib2Type, kAttrib2Location, },
      { kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location, },
    };
    static UniformInfo uniforms[] = {
      { kUniform1Name, kUniform1Size, kUniform1Type,
        kUniform1FakeLocation, kUniform1RealLocation,
        kUniform1DesiredLocation },
      { kUniform2Name, kUniform2Size, kUniform2Type,
        kUniform2FakeLocation, kUniform2RealLocation,
        kUniform2DesiredLocation },
      { kUniform3Name, kUniform3Size, kUniform3Type,
        kUniform3FakeLocation, kUniform3RealLocation,
        kUniform3DesiredLocation },
      { kUniform4Name, kUniform4Size, kUniform4Type,
        kUniform4FakeLocation, kUniform4RealLocation,
        kUniform4DesiredLocation },
      { kUniform5Name, kUniform5Size, kUniform5Type,
        kUniform5FakeLocation, kUniform5RealLocation,
        kUniform5DesiredLocation },
      { kUniform6Name, kUniform6Size, kUniform6Type,
        kUniform6FakeLocation, kUniform6RealLocation,
        kUniform6DesiredLocation },
      { kUniform7Name, kUniform7Size, kUniform7Type,
        kUniform7FakeLocation, kUniform7RealLocation,
        kUniform7DesiredLocation },
      { kUniform8Name, kUniform8Size, kUniform8Type,
        kUniform8FakeLocation, kUniform8RealLocation,
        kUniform8DesiredLocation },
    };
    SetupShader(attribs, std::size(attribs), uniforms, std::size(uniforms),
                client_program_id_, kServiceProgramId, client_vertex_shader_id_,
                kServiceVertexShaderId, client_fragment_shader_id_,
                kServiceFragmentShaderId);
  }

  {
    EXPECT_CALL(*gl_, UseProgram(kServiceProgramId))
        .Times(1)
        .RetiresOnSaturation();
    cmds::UseProgram cmd;
    cmd.Init(client_program_id_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
}

void GLES2DecoderTestBase::SetupCubemapProgram() {
  {
    static AttribInfo attribs[] = {
      { kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location, },
      { kAttrib2Name, kAttrib2Size, kAttrib2Type, kAttrib2Location, },
      { kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location, },
    };
    static UniformInfo uniforms[] = {
      { kUniform1Name, kUniform1Size, kUniformCubemapType,
        kUniform1FakeLocation, kUniform1RealLocation,
        kUniform1DesiredLocation, },
      { kUniform2Name, kUniform2Size, kUniform2Type,
        kUniform2FakeLocation, kUniform2RealLocation,
        kUniform2DesiredLocation, },
      { kUniform3Name, kUniform3Size, kUniform3Type,
        kUniform3FakeLocation, kUniform3RealLocation,
        kUniform3DesiredLocation, },
      { kUniform4Name, kUniform4Size, kUniform4Type,
        kUniform4FakeLocation, kUniform4RealLocation,
        kUniform4DesiredLocation, },
      { kUniform5Name, kUniform5Size, kUniform5Type,
        kUniform5FakeLocation, kUniform5RealLocation,
        kUniform5DesiredLocation },
      { kUniform6Name, kUniform6Size, kUniform6Type,
        kUniform6FakeLocation, kUniform6RealLocation,
        kUniform6DesiredLocation },
      { kUniform7Name, kUniform7Size, kUniform7Type,
        kUniform7FakeLocation, kUniform7RealLocation,
        kUniform7DesiredLocation },
    };
    SetupShader(attribs, std::size(attribs), uniforms, std::size(uniforms),
                client_program_id_, kServiceProgramId, client_vertex_shader_id_,
                kServiceVertexShaderId, client_fragment_shader_id_,
                kServiceFragmentShaderId);
  }

  {
    EXPECT_CALL(*gl_, UseProgram(kServiceProgramId))
        .Times(1)
        .RetiresOnSaturation();
    cmds::UseProgram cmd;
    cmd.Init(client_program_id_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
}

void GLES2DecoderTestBase::SetupSamplerExternalProgram() {
  {
    static AttribInfo attribs[] = {
      { kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location, },
      { kAttrib2Name, kAttrib2Size, kAttrib2Type, kAttrib2Location, },
      { kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location, },
    };
    static UniformInfo uniforms[] = {
      { kUniform1Name, kUniform1Size, kUniformSamplerExternalType,
        kUniform1FakeLocation, kUniform1RealLocation,
        kUniform1DesiredLocation, },
      { kUniform2Name, kUniform2Size, kUniform2Type,
        kUniform2FakeLocation, kUniform2RealLocation,
        kUniform2DesiredLocation, },
      { kUniform3Name, kUniform3Size, kUniform3Type,
        kUniform3FakeLocation, kUniform3RealLocation,
        kUniform3DesiredLocation, },
      { kUniform4Name, kUniform4Size, kUniform4Type,
        kUniform4FakeLocation, kUniform4RealLocation,
        kUniform4DesiredLocation, },
      { kUniform5Name, kUniform5Size, kUniform5Type,
        kUniform5FakeLocation, kUniform5RealLocation,
        kUniform5DesiredLocation },
      { kUniform6Name, kUniform6Size, kUniform6Type,
        kUniform6FakeLocation, kUniform6RealLocation,
        kUniform6DesiredLocation },
      { kUniform7Name, kUniform7Size, kUniform7Type,
        kUniform7FakeLocation, kUniform7RealLocation,
        kUniform7DesiredLocation },
    };
    SetupShader(attribs, std::size(attribs), uniforms, std::size(uniforms),
                client_program_id_, kServiceProgramId, client_vertex_shader_id_,
                kServiceVertexShaderId, client_fragment_shader_id_,
                kServiceFragmentShaderId);
  }

  {
    EXPECT_CALL(*gl_, UseProgram(kServiceProgramId))
        .Times(1)
        .RetiresOnSaturation();
    cmds::UseProgram cmd;
    cmd.Init(client_program_id_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
}

void GLES2DecoderWithShaderTestBase::TearDown() {
  GLES2DecoderTestBase::TearDown();
}

void GLES2DecoderTestBase::SetupShader(
    GLES2DecoderTestBase::AttribInfo* attribs, size_t num_attribs,
    GLES2DecoderTestBase::UniformInfo* uniforms, size_t num_uniforms,
    GLuint program_client_id, GLuint program_service_id,
    GLuint vertex_shader_client_id, GLuint vertex_shader_service_id,
    GLuint fragment_shader_client_id, GLuint fragment_shader_service_id) {
  static TestHelper::ProgramOutputInfo kProgramOutputsESSL1[] = {{
      kOutputVariable1Name, kOutputVariable1Size, kOutputVariable1Type,
      kOutputVariable1ColorName, kOutputVariable1Index,
  }};
  static TestHelper::ProgramOutputInfo kProgramOutputsESSL3[] = {{
      kOutputVariable1NameESSL3, kOutputVariable1Size, kOutputVariable1Type,
      kOutputVariable1ColorName, kOutputVariable1Index,
  }};
  TestHelper::ProgramOutputInfo* program_outputs =
      shader_language_version_ == 100 ? kProgramOutputsESSL1
                                      : kProgramOutputsESSL3;
  const size_t kNumProgramOutputs = 1;
  const int kNumUniformBlocks = 2;
  const int kUniformBlockBinding[] = { 0, 1 };
  const int kUniformBlockDataSize[] = { 32, 16 };

  {
    InSequence s;

    EXPECT_CALL(*gl_,
                AttachShader(program_service_id, vertex_shader_service_id))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
                AttachShader(program_service_id, fragment_shader_service_id))
        .Times(1)
        .RetiresOnSaturation();

    TestHelper::SetupShaderExpectationsWithVaryings(
        gl_.get(), group_->feature_info(), attribs, num_attribs, uniforms,
        num_uniforms, nullptr, 0, program_outputs, kNumProgramOutputs,
        program_service_id);
  }

  DoCreateShader(
      GL_VERTEX_SHADER, vertex_shader_client_id, vertex_shader_service_id);
  DoCreateShader(
      GL_FRAGMENT_SHADER, fragment_shader_client_id,
      fragment_shader_service_id);

  TestHelper::SetShaderStates(gl_.get(), GetShader(vertex_shader_client_id),
                              true, nullptr, nullptr, &shader_language_version_,
                              nullptr, nullptr, nullptr, nullptr, nullptr,
                              nullptr);

  OutputVariableList frag_output_variable_list;
  frag_output_variable_list.push_back(TestHelper::ConstructOutputVariable(
      program_outputs[0].type, program_outputs[0].size, GL_MEDIUM_FLOAT, true,
      program_outputs[0].name));

  TestHelper::SetShaderStates(gl_.get(), GetShader(fragment_shader_client_id),
                              true, nullptr, nullptr, &shader_language_version_,
                              nullptr, nullptr, nullptr, nullptr,
                              &frag_output_variable_list, nullptr);

  cmds::AttachShader attach_cmd;
  attach_cmd.Init(program_client_id, vertex_shader_client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(attach_cmd));

  attach_cmd.Init(program_client_id, fragment_shader_client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(attach_cmd));

  if (shader_language_version_ == 300) {
    EXPECT_CALL(*gl_, GetProgramiv(
        program_service_id, GL_ACTIVE_UNIFORM_BLOCKS, _))
        .WillOnce(SetArgPointee<2>(kNumUniformBlocks))
        .RetiresOnSaturation();
    for (int ii = 0; ii < kNumUniformBlocks; ++ii) {
      EXPECT_CALL(*gl_,
                  GetActiveUniformBlockiv(program_service_id, ii,
                                          GL_UNIFORM_BLOCK_BINDING, _))
          .WillOnce(SetArgPointee<3>(kUniformBlockBinding[ii]))
          .RetiresOnSaturation();
      EXPECT_CALL(*gl_,
                  GetActiveUniformBlockiv(program_service_id, ii,
                                          GL_UNIFORM_BLOCK_DATA_SIZE, _))
          .WillOnce(SetArgPointee<3>(kUniformBlockDataSize[ii]))
          .RetiresOnSaturation();
    }
  }

  cmds::LinkProgram link_cmd;
  link_cmd.Init(program_client_id);

  EXPECT_EQ(error::kNoError, ExecuteCmd(link_cmd));
}

void GLES2DecoderTestBase::DoEnableDisable(GLenum cap, bool enable) {
  SetupExpectationsForEnableDisable(cap, enable);
  if (enable) {
    cmds::Enable cmd;
    cmd.Init(cap);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  } else {
    cmds::Disable cmd;
    cmd.Init(cap);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
}

void GLES2DecoderTestBase::SetDriverVertexAttribEnabled(GLint index,
                                                        bool enable) {
  DCHECK(index < static_cast<GLint>(attribs_enabled_.size()));
  bool already_enabled = attribs_enabled_[index];
  if (already_enabled != enable) {
    attribs_enabled_[index] = enable;
    if (enable) {
      EXPECT_CALL(*gl_, EnableVertexAttribArray(index))
          .Times(1)
          .RetiresOnSaturation();
    } else {
      EXPECT_CALL(*gl_, DisableVertexAttribArray(index))
          .Times(1)
          .RetiresOnSaturation();
    }
  }
}

void GLES2DecoderTestBase::DoEnableVertexAttribArray(GLint index) {
  SetDriverVertexAttribEnabled(index, true);
  cmds::EnableVertexAttribArray cmd;
  cmd.Init(index);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoBufferData(GLenum target, GLsizei size) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BufferData(target, size, _, GL_STREAM_DRAW))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::BufferData cmd;
  cmd.Init(target, size, 0, 0, GL_STREAM_DRAW);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoBufferSubData(
    GLenum target, GLint offset, GLsizei size, const void* data) {
  EXPECT_CALL(*gl_,
              BufferSubData(target, offset, size, shared_memory_address_.get()))
      .Times(1)
      .RetiresOnSaturation();
  memcpy(shared_memory_address_, data, size);
  cmds::BufferSubData cmd;
  cmd.Init(target, offset, size, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoScissor(GLint x,
                                     GLint y,
                                     GLsizei width,
                                     GLsizei height) {
  EXPECT_CALL(*gl_, Scissor(x, y, width, height))
      .Times(1)
      .RetiresOnSaturation();
  cmds::Scissor cmd;
  cmd.Init(x, y, width, height);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoPixelStorei(GLenum pname, GLint param) {
  EXPECT_CALL(*gl_, PixelStorei(pname, param))
      .Times(1)
      .RetiresOnSaturation();
  cmds::PixelStorei cmd;
  cmd.Init(pname, param);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::SetupVertexBuffer() {
  DoEnableVertexAttribArray(1);
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  DoBufferData(GL_ARRAY_BUFFER, kNumVertices * 2 * sizeof(GLfloat));
}

void GLES2DecoderTestBase::SetupAllNeededVertexBuffers() {
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  DoBufferData(GL_ARRAY_BUFFER, kNumVertices * 16 * sizeof(float));
  DoEnableVertexAttribArray(0);
  DoEnableVertexAttribArray(1);
  DoEnableVertexAttribArray(2);
  DoVertexAttribPointer(0, 2, GL_FLOAT, 0, 0);
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  DoVertexAttribPointer(2, 2, GL_FLOAT, 0, 0);
}

void GLES2DecoderTestBase::SetupIndexBuffer() {
  DoBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
               client_element_buffer_id_,
               kServiceElementBufferId);
  static const GLshort indices[] = {100, 1, 2, 3, 4, 5, 6, 7, 100, 9};
  static_assert(std::size(indices) == kNumIndices,
                "indices should have kNumIndices elements");
  DoBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices));
  DoBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, 2, indices);
  DoBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 2, sizeof(indices) - 2, &indices[1]);
}

void GLES2DecoderTestBase::SetupTexture() {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               shared_memory_id_, kSharedMemoryOffset);
}

void GLES2DecoderTestBase::SetupSampler() {
  DoBindSampler(0, client_sampler_id_, kServiceSamplerId);
}

void GLES2DecoderTestBase::DeleteVertexBuffer() {
  DoDeleteBuffer(client_buffer_id_, kServiceBufferId);
}

void GLES2DecoderTestBase::DeleteIndexBuffer() {
  DoDeleteBuffer(client_element_buffer_id_, kServiceElementBufferId);
}

void GLES2DecoderTestBase::SetupMockGLBehaviors() {
  ON_CALL(*gl_, BindVertexArrayOES(_))
      .WillByDefault(Invoke(
          &gl_states_,
          &GLES2DecoderTestBase::MockGLStates::OnBindVertexArrayOES));
  ON_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, _))
      .WillByDefault(WithArg<1>(Invoke(
          &gl_states_,
          &GLES2DecoderTestBase::MockGLStates::OnBindArrayBuffer)));
  ON_CALL(*gl_, VertexAttribPointer(_, _, _, _, _, nullptr))
      .WillByDefault(InvokeWithoutArgs(
          &gl_states_,
          &GLES2DecoderTestBase::MockGLStates::OnVertexAttribNullPointer));
}

void GLES2DecoderWithShaderTestBase::SetUp() {
  GLES2DecoderTestBase::SetUp();
  SetupDefaultProgram();
}

void GLES2DecoderTestBase::DoInitializeDiscardableTextureCHROMIUM(
    GLuint texture_id) {
  scoped_refptr<gpu::Buffer> buffer =
      command_buffer_service_->GetTransferBuffer(shared_memory_id_);
  ClientDiscardableHandle handle(buffer, 0, shared_memory_id_);

  cmds::InitializeDiscardableTextureCHROMIUM cmd;
  cmd.Init(texture_id, shared_memory_id_, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoUnlockDiscardableTextureCHROMIUM(
    GLuint texture_id) {
  cmds::UnlockDiscardableTextureCHROMIUM cmd;
  cmd.Init(texture_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoLockDiscardableTextureCHROMIUM(GLuint texture_id) {
  cmds::LockDiscardableTextureCHROMIUM cmd;
  cmd.Init(texture_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

namespace {

GpuPreferences GenerateGpuPreferencesForPassthroughTests() {
  GpuPreferences preferences;
  preferences.use_passthrough_cmd_decoder = true;
  return preferences;
}
}  // anonymous namespace

GLES2DecoderPassthroughTestBase::GLES2DecoderPassthroughTestBase(
    ContextType context_type)
    : gpu_preferences_(GenerateGpuPreferencesForPassthroughTests()),
      shader_translator_cache_(gpu_preferences_),
      discardable_manager_(gpu_preferences_),
      passthrough_discardable_manager_(gpu_preferences_) {
  context_creation_attribs_.context_type = context_type;
}

GLES2DecoderPassthroughTestBase::~GLES2DecoderPassthroughTestBase() = default;

void GLES2DecoderPassthroughTestBase::OnConsoleMessage(
    int32_t id,
    const std::string& message) {}
void GLES2DecoderPassthroughTestBase::CacheBlob(gpu::GpuDiskCacheType type,
                                                const std::string& key,
                                                const std::string& blob) {}
void GLES2DecoderPassthroughTestBase::OnFenceSyncRelease(uint64_t release) {}
void GLES2DecoderPassthroughTestBase::OnDescheduleUntilFinished() {}
void GLES2DecoderPassthroughTestBase::OnRescheduleAfterFinished() {}
void GLES2DecoderPassthroughTestBase::OnSwapBuffers(uint64_t swap_id,
                                                    uint32_t flags) {}
bool GLES2DecoderPassthroughTestBase::ShouldYield() {
  return false;
}

void GLES2DecoderPassthroughTestBase::SetUp() {
  base::CommandLine::Init(0, nullptr);
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kUseGL,
                                  gl::kGLImplementationANGLEName);
  command_line->AppendSwitchASCII(switches::kUseANGLE,
                                  gl::kANGLEImplementationNullName);

#if BUILDFLAG(IS_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForGPU(params);
#endif

  context_creation_attribs_.bind_generates_resource = true;

  gl::init::InitializeStaticGLBindingsImplementation(
      gl::GLImplementationParts(gl::ANGLEImplementation::kNull));
  display_ = gl::init::InitializeGLOneOffPlatformImplementation(
      /*disable_gl_drawing=*/false,
      /*init_extensions=*/true,
      /*gpu_preference=*/gl::GpuPreference::kDefault);

  // Ensure we're running with Null Backend.
  ASSERT_EQ(gl::GetANGLEImplementation(), gl::ANGLEImplementation::kNull);

  scoped_refptr<gles2::FeatureInfo> feature_info = new gles2::FeatureInfo();
  group_ = new gles2::ContextGroup(
      gpu_preferences_, true, nullptr /* memory_tracker */,
      &shader_translator_cache_, &framebuffer_completeness_cache_, feature_info,
      context_creation_attribs_.bind_generates_resource,
      nullptr /* progress_reporter */, GpuFeatureInfo(), &discardable_manager_,
      &passthrough_discardable_manager_, &shared_image_manager_);

  surface_ = gl::init::CreateOffscreenGLSurface(display_, gfx::Size(4, 4));
  context_ =
      gl::init::CreateGLContext(nullptr, surface_.get(),
                                GenerateGLContextAttribsForDecoder(
                                    context_creation_attribs_, group_.get()));
  context_->MakeCurrent(surface_.get());

  command_buffer_service_ = std::make_unique<FakeCommandBufferServiceBase>();

  decoder_ = std::make_unique<GLES2DecoderPassthroughImpl>(
      this, command_buffer_service_.get(), &outputter_, group_.get());

  // Don't request any optional extensions at startup, individual tests will
  // request what they need.
  decoder_->SetOptionalExtensionsRequestedForTesting(false);

  ASSERT_EQ(
      group_->Initialize(decoder_.get(), context_creation_attribs_.context_type,
                         DisallowedFeatures()),
      gpu::ContextResult::kSuccess);

  // We need command buffer to emulate default framebuffer is the GLSurface is
  // surfaceless.
  const bool offscreen = surface_->IsSurfaceless();
  ASSERT_EQ(
      decoder_->Initialize(surface_, context_, offscreen, DisallowedFeatures(),
                           context_creation_attribs_),
      gpu::ContextResult::kSuccess);

  scoped_refptr<gpu::Buffer> buffer =
      command_buffer_service_->CreateTransferBufferHelper(kSharedBufferSize,
                                                          &shared_memory_id_);
  shared_memory_offset_ = kSharedMemoryOffset;
  shared_memory_address_ =
      static_cast<int8_t*>(buffer->memory()) + shared_memory_offset_;
  shared_memory_base_ = buffer->memory();
  shared_memory_size_ = kSharedBufferSize - shared_memory_offset_;

  decoder_->MakeCurrent();
  decoder_->BeginDecoding();
}

void GLES2DecoderPassthroughTestBase::TearDown() {
  surface_ = nullptr;
  context_ = nullptr;
  decoder_->EndDecoding();
  decoder_->Destroy(!decoder_->WasContextLost());
  group_->Destroy(decoder_.get(), false);
  decoder_.reset();
  group_ = nullptr;

  // Drop unowned references to buffer memory before service destroys them.
  shared_memory_address_ = nullptr;
  shared_memory_base_ = nullptr;

  command_buffer_service_.reset();
  gl::init::ShutdownGL(display_, false);
}

void GLES2DecoderPassthroughTestBase::SetBucketData(uint32_t bucket_id,
                                                    const void* data,
                                                    size_t data_size) {
  DCHECK(data || data_size == 0);
  {
    cmd::SetBucketSize cmd;
    cmd.Init(bucket_id, static_cast<uint32_t>(data_size));
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
  if (data) {
    memcpy(shared_memory_address_, data, data_size);
    cmd::SetBucketData cmd;
    cmd.Init(bucket_id, 0, static_cast<uint32_t>(data_size), shared_memory_id_,
             kSharedMemoryOffset);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    memset(shared_memory_address_, 0, data_size);
  }
}

GLint GLES2DecoderPassthroughTestBase::GetGLError() {
  cmds::GetError cmd;
  cmd.Init(shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  return static_cast<GLint>(*GetSharedMemoryAs<GLenum*>());
}

void GLES2DecoderPassthroughTestBase::DoRequestExtension(
    const char* extension) {
  DCHECK(extension != nullptr);

  uint32_t bucket_id = 0;
  SetBucketData(bucket_id, extension, strlen(extension) + 1);

  cmds::RequestExtensionCHROMIUM cmd;
  cmd.Init(bucket_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderPassthroughTestBase::DoBindBuffer(GLenum target,
                                                   GLuint client_id) {
  cmds::BindBuffer cmd;
  cmd.Init(target, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderPassthroughTestBase::DoDeleteBuffer(GLuint client_id) {
  GenHelper<cmds::DeleteBuffersImmediate>(client_id);
}

void GLES2DecoderPassthroughTestBase::DoBufferData(GLenum target,
                                                   GLsizei size,
                                                   const void* data,
                                                   GLenum usage) {
  cmds::BufferData cmd;
  if (data) {
    EXPECT_TRUE(size >= 0);
    EXPECT_LT(static_cast<size_t>(size),
              kSharedBufferSize - kSharedMemoryOffset);
    memcpy(shared_memory_address_, data, size);
    cmd.Init(target, size, shared_memory_id_, shared_memory_offset_, usage);
  } else {
    cmd.Init(target, size, 0, 0, usage);
  }
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderPassthroughTestBase::DoBufferSubData(GLenum target,
                                                      GLint offset,
                                                      GLsizeiptr size,
                                                      const void* data) {
  memcpy(shared_memory_address_, data, size);
  cmds::BufferSubData cmd;
  cmd.Init(target, offset, size, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderPassthroughTestBase::DoGenTexture(GLuint client_id) {
  GenHelper<cmds::GenTexturesImmediate>(client_id);
}

bool GLES2DecoderPassthroughTestBase::DoIsTexture(GLuint client_id) {
  return IsObjectHelper<cmds::IsTexture>(client_id);
}

void GLES2DecoderPassthroughTestBase::DoBindTexture(GLenum target,
                                                    GLuint client_id) {
  cmds::BindTexture cmd;
  cmd.Init(target, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderPassthroughTestBase::DoDeleteTexture(GLuint client_id) {
  GenHelper<cmds::DeleteTexturesImmediate>(client_id);
}

void GLES2DecoderPassthroughTestBase::DoTexImage2D(
    GLenum target,
    GLint level,
    GLenum internal_format,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    uint32_t shared_memory_id,
    uint32_t shared_memory_offset) {
  cmds::TexImage2D cmd;
  cmd.Init(target, level, internal_format, width, height, format, type,
           shared_memory_id, shared_memory_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderPassthroughTestBase::DoBindFramebuffer(GLenum target,
                                                        GLuint client_id) {
  cmds::BindFramebuffer cmd;
  cmd.Init(target, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderPassthroughTestBase::DoFramebufferTexture2D(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture_client_id,
    GLint level) {
  cmds::FramebufferTexture2D cmd;
  cmd.Init(target, attachment, textarget, texture_client_id, level);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderPassthroughTestBase::DoFramebufferRenderbuffer(
    GLenum target,
    GLenum attachment,
    GLenum renderbuffertarget,
    GLuint renderbuffer) {
  cmds::FramebufferRenderbuffer cmd;
  cmd.Init(target, attachment, renderbuffertarget, renderbuffer);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderPassthroughTestBase::DoBindRenderbuffer(GLenum target,
                                                         GLuint client_id) {
  cmds::BindRenderbuffer cmd;
  cmd.Init(target, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderPassthroughTestBase::DoGetIntegerv(GLenum pname,
                                                    GLint* result,
                                                    size_t num_results) {
  cmds::GetIntegerv cmd;
  cmd.Init(pname, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  cmds::GetIntegerv::Result* cmd_result =
      GetSharedMemoryAs<cmds::GetIntegerv::Result*>();
  DCHECK(static_cast<size_t>(cmd_result->GetNumResults()) >= num_results);
  std::copy(cmd_result->GetData(), cmd_result->GetData() + num_results, result);
}

void GLES2DecoderPassthroughTestBase::DoInitializeDiscardableTextureCHROMIUM(
    GLuint client_id) {
  int32_t shmem_id = 0;
  scoped_refptr<gpu::Buffer> buffer =
      command_buffer_service_->CreateTransferBufferHelper(sizeof(uint32_t),
                                                          &shmem_id);
  ClientDiscardableHandle handle(buffer, 0, shmem_id);

  cmds::InitializeDiscardableTextureCHROMIUM cmd;
  cmd.Init(client_id, shmem_id, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderPassthroughTestBase::DoUnlockDiscardableTextureCHROMIUM(
    GLuint client_id) {
  cmds::UnlockDiscardableTextureCHROMIUM cmd;
  cmd.Init(client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderPassthroughTestBase::DoLockDiscardableTextureCHROMIUM(
    GLuint client_id) {
  cmds::LockDiscardableTextureCHROMIUM cmd;
  cmd.Init(client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const size_t GLES2DecoderPassthroughTestBase::kSharedBufferSize;
const uint32_t GLES2DecoderPassthroughTestBase::kSharedMemoryOffset;
const uint32_t GLES2DecoderPassthroughTestBase::kInvalidSharedMemoryOffset;
const int32_t GLES2DecoderPassthroughTestBase::kInvalidSharedMemoryId;

const uint32_t GLES2DecoderPassthroughTestBase::kNewClientId;
const GLuint GLES2DecoderPassthroughTestBase::kClientBufferId;
const GLuint GLES2DecoderPassthroughTestBase::kClientTextureId;
const GLuint GLES2DecoderPassthroughTestBase::kClientFramebufferId;
const GLuint GLES2DecoderPassthroughTestBase::kClientRenderbufferId;
#endif

}  // namespace gles2
}  // namespace gpu
