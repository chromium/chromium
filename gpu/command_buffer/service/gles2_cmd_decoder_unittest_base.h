// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_BASE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_BASE_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gl_context_mock.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"
#include "gpu/command_buffer/service/gles2_query_manager.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/passthrough_discardable_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/sampler_manager.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/transform_feedback_manager.h"
#include "gpu/command_buffer/service/vertex_array_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {
class MemoryTracker;

namespace gles2 {
class MockCopyTextureResourceManager;
class MockCopyTexImageResourceManager;

class GLES2DecoderTestBase : public ::testing::TestWithParam<bool>,
                             public DecoderClient {
 public:
  GLES2DecoderTestBase();
  ~GLES2DecoderTestBase() override;

  void OnConsoleMessage(int32_t id, const std::string& message) override;
  void CacheBlob(gpu::GpuDiskCacheType type,
                 const std::string& key,
                 const std::string& blob) override;
  void OnFenceSyncRelease(uint64_t release) override;
  void OnDescheduleUntilFinished() override;
  void OnRescheduleAfterFinished() override;
  void OnSwapBuffers(uint64_t swap_id, uint32_t flags) override;
  void ScheduleGrContextCleanup() override {}
  void HandleReturnData(base::span<const uint8_t> data) override {}
  bool ShouldYield() override;

  // Template to call glGenXXX functions.
  template <typename T>
  void GenHelper(GLuint client_id) {
    int8_t buffer[sizeof(T) + sizeof(client_id)];
    T& cmd = *reinterpret_cast<T*>(&buffer);
    cmd.Init(1, &client_id);
    EXPECT_EQ(error::kNoError,
              ExecuteImmediateCmd(cmd, sizeof(client_id)));
  }

  // This template exists solely so we can specialize it for
  // certain commands.
  template <typename T, int id>
  void SpecializedSetup(bool valid) {
  }

  template <typename T>
  T* GetImmediateAs() {
    return reinterpret_cast<T*>(immediate_buffer_);
  }

  void ClearSharedMemory() {
    memset(shared_memory_base_, kInitialMemoryValue, kSharedBufferSize);
  }

  void SetUp() override;
  void TearDown() override;

  template <typename T>
  error::Error ExecuteCmd(const T& cmd) {
    static_assert(T::kArgFlags == cmd::kFixed,
                  "T::kArgFlags should equal cmd::kFixed");
    int entries_processed = 0;
    return decoder_->DoCommands(1, (const void*)&cmd,
                                ComputeNumEntries(sizeof(cmd)),
                                &entries_processed);
  }

  template <typename T>
  error::Error ExecuteImmediateCmd(const T& cmd, size_t data_size) {
    static_assert(T::kArgFlags == cmd::kAtLeastN,
                  "T::kArgFlags should equal cmd::kAtLeastN");
    int entries_processed = 0;
    return decoder_->DoCommands(1, (const void*)&cmd,
                                ComputeNumEntries(sizeof(cmd) + data_size),
                                &entries_processed);
  }

  template <typename T>
  T GetSharedMemoryAs() {
    return reinterpret_cast<T>(shared_memory_address_.get());
  }

  template <typename T>
  T GetSharedMemoryAsWithOffset(uint32_t offset) {
    void* ptr =
        reinterpret_cast<int8_t*>(shared_memory_address_.get()) + offset;
    return reinterpret_cast<T>(ptr);
  }

  Buffer* GetBuffer(GLuint client_id) {
    return group_->buffer_manager()->GetBuffer(client_id);
  }

  Framebuffer* GetFramebuffer(GLuint client_id) {
    return decoder_->GetFramebufferManager()->GetFramebuffer(client_id);
  }

  Renderbuffer* GetRenderbuffer(GLuint client_id) {
    return group_->renderbuffer_manager()->GetRenderbuffer(client_id);
  }

  TextureRef* GetTexture(GLuint client_id) {
    return group_->texture_manager()->GetTexture(client_id);
  }

  Shader* GetShader(GLuint client_id) {
    return group_->shader_manager()->GetShader(client_id);
  }

  Program* GetProgram(GLuint client_id) {
    return group_->program_manager()->GetProgram(client_id);
  }

  QueryManager::Query* GetQueryInfo(GLuint client_id) {
    return decoder_->GetQueryManager()->GetQuery(client_id);
  }

  Sampler* GetSampler(GLuint client_id) {
    return group_->sampler_manager()->GetSampler(client_id);
  }

  TransformFeedback* GetTransformFeedback(GLuint client_id) {
    return decoder_->GetTransformFeedbackManager()->GetTransformFeedback(
        client_id);
  }

  bool GetSyncServiceId(GLuint client_id, GLsync* service_id) const {
    return group_->GetSyncServiceId(client_id, service_id);
  }

  // This name doesn't match the underlying function, but doing it this way
  // prevents the need to special-case the unit test generation
  VertexAttribManager* GetVertexArrayInfo(GLuint client_id) {
    return decoder_->GetVertexArrayManager()->GetVertexAttribManager(client_id);
  }

  ProgramManager* program_manager() {
    return group_->program_manager();
  }

  FeatureInfo* feature_info() {
    return group_->feature_info();
  }

  FramebufferCompletenessCache* framebuffer_completeness_cache() const {
    return group_->framebuffer_completeness_cache();
  }

  FramebufferManager* GetFramebufferManager() {
    return decoder_->GetFramebufferManager();
  }

  void DoCreateProgram(GLuint client_id, GLuint service_id);
  void DoCreateShader(GLenum shader_type, GLuint client_id, GLuint service_id);
  void DoFenceSync(GLuint client_id, GLuint service_id);
  void DoCreateSampler(GLuint client_id, GLuint service_id);
  void DoCreateTransformFeedback(GLuint client_id, GLuint service_id);

  void SetBucketData(uint32_t bucket_id, const void* data, uint32_t data_size);
  void SetBucketAsCString(uint32_t bucket_id, const char* str);
  // If we want a valid bucket, just set |count_in_header| as |count|,
  // and set |str_end| as 0.
  void SetBucketAsCStrings(uint32_t bucket_id,
                           GLsizei count,
                           const char** str,
                           GLsizei count_in_header,
                           char str_end);

  void set_memory_tracker(std::unique_ptr<MemoryTracker> memory_tracker) {
    memory_tracker_ = std::move(memory_tracker);
  }

  struct InitState {
    InitState();
    InitState(const InitState& other);
    InitState& operator=(const InitState& other);

    std::string extensions;
    std::string gl_version = "OpenGL ES 3.0";
    bool has_alpha = false;
    bool has_depth = false;
    bool has_stencil = false;
    bool request_alpha = false;
    bool request_depth = false;
    bool request_stencil = false;
    bool bind_generates_resource = false;
    bool lose_context_when_out_of_memory = false;
    bool lose_context_on_init = false;
    bool use_native_vao = true;
    ContextType context_type = CONTEXT_TYPE_OPENGLES2;
  };

  void InitDecoder(const InitState& init);
  void InitDecoderWithWorkarounds(const InitState& init,
                                  const GpuDriverBugWorkarounds& workarounds);
  ContextResult MaybeInitDecoderWithWorkarounds(
      const InitState& init,
      const GpuDriverBugWorkarounds& workarounds);

  void ResetDecoder();

  const ContextGroup& group() const {
    return *group_.get();
  }

  void LoseContexts(error::ContextLostReason reason) const {
    group_->LoseContexts(reason);
  }

  error::ContextLostReason GetContextLostReason() const {
    return command_buffer_service_->GetState().context_lost_reason;
  }

  ::testing::StrictMock<::gl::MockGLInterface>* GetGLMock() const {
    return gl_.get();
  }

  GLES2Decoder* GetDecoder() const {
    return decoder_.get();
  }

  uint32_t GetAndClearBackbufferClearBitsForTest() const {
    return decoder_->GetAndClearBackbufferClearBitsForTest();
  }

  SharedImageManager* GetSharedImageManager() { return &shared_image_manager_; }

  typedef TestHelper::AttribInfo AttribInfo;
  typedef TestHelper::UniformInfo UniformInfo;

  void SetupShader(
      AttribInfo* attribs, size_t num_attribs,
      UniformInfo* uniforms, size_t num_uniforms,
      GLuint client_id, GLuint service_id,
      GLuint vertex_shader_client_id, GLuint vertex_shader_service_id,
      GLuint fragment_shader_client_id, GLuint fragment_shader_service_id);

  // Setups up a shader for testing glUniform.
  void SetupShaderForUniform(GLenum uniform_type);
  void SetupDefaultProgram();
  void SetupCubemapProgram();
  void SetupSamplerExternalProgram();
  void SetupTexture();

  // Sets up a sampler on texture unit 0 for certain ES3-specific tests.
  void SetupSampler();

  // Note that the error is returned as GLint instead of GLenum.
  // This is because there is a mismatch in the types of GLenum and
  // the error values GL_NO_ERROR, GL_INVALID_ENUM, etc. GLenum is
  // typedef'd as unsigned int while the error values are defined as
  // integers. This is problematic for template functions such as
  // EXPECT_EQ that expect both types to be the same.
  GLint GetGLError();

  void DoBindBuffer(GLenum target, GLuint client_id, GLuint service_id);
  void DoBindFramebuffer(GLenum target, GLuint client_id, GLuint service_id);
  void DoBindRenderbuffer(GLenum target, GLuint client_id, GLuint service_id);
  void SetupExpectationsForInternalFormatSampleCountsHelper(
      GLenum target,
      GLenum internal_format,
      GLint expected_num_sample_counts,
      GLint expected_sample0);
  void DoRenderbufferStorageMultisampleCHROMIUM(GLenum target,
                                                GLsizei samples,
                                                GLenum internal_format,
                                                GLsizei width,
                                                GLsizei height,
                                                bool expect_bind);
  void RestoreRenderbufferBindings();
  void EnsureRenderbufferBound(bool expect_bind);
  void DoBindTexture(GLenum target, GLuint client_id, GLuint service_id);
  void DoBindVertexArrayOES(GLuint client_id, GLuint service_id);
  void DoBindSampler(GLuint unit, GLuint client_id, GLuint service_id);
  void DoBindTransformFeedback(
      GLenum target, GLuint client_id, GLuint service_id);

  bool DoIsBuffer(GLuint client_id);
  bool DoIsFramebuffer(GLuint client_id);
  bool DoIsProgram(GLuint client_id);
  bool DoIsRenderbuffer(GLuint client_id);
  bool DoIsShader(GLuint client_id);
  bool DoIsTexture(GLuint client_id);

  void DoDeleteBuffer(GLuint client_id, GLuint service_id);
  void DoDeleteFramebuffer(
      GLuint client_id, GLuint service_id,
      bool reset_draw, GLenum draw_target, GLuint draw_id,
      bool reset_read, GLenum read_target, GLuint read_id);
  void DoDeleteProgram(GLuint client_id, GLuint service_id);
  void DoDeleteRenderbuffer(GLuint client_id, GLuint service_id);
  void DoDeleteShader(GLuint client_id, GLuint service_id);
  void DoDeleteTexture(GLuint client_id, GLuint service_id);
  void DoDeleteSampler(GLuint client_id, GLuint service_id);
  void DoDeleteTransformFeedback(GLuint client_id, GLuint service_id);

  void DoCompressedTexImage2D(GLenum target,
                              GLint level,
                              GLenum format,
                              GLsizei width,
                              GLsizei height,
                              GLint border,
                              GLsizei size,
                              uint32_t bucket_id);
  void DoTexImage2D(GLenum target,
                    GLint level,
                    GLenum internal_format,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    uint32_t shared_memory_id,
                    uint32_t shared_memory_offset);
  void DoTexImage2DConvertInternalFormat(GLenum target,
                                         GLint level,
                                         GLenum requested_internal_format,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border,
                                         GLenum format,
                                         GLenum type,
                                         uint32_t shared_memory_id,
                                         uint32_t shared_memory_offset,
                                         GLenum expected_internal_format);
  void DoTexImage3D(GLenum target,
                    GLint level,
                    GLenum internal_format,
                    GLsizei width,
                    GLsizei height,
                    GLsizei depth,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    uint32_t shared_memory_id,
                    uint32_t shared_memory_offset);
  void DoCopyTexImage2D(GLenum target,
                        GLint level,
                        GLenum internal_format,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLint border);
  void DoRenderbufferStorage(GLenum target,
                             GLenum internal_format,
                             GLsizei width,
                             GLsizei height,
                             GLenum error);
  void DoFramebufferRenderbuffer(
      GLenum target,
      GLenum attachment,
      GLenum renderbuffer_target,
      GLuint renderbuffer_client_id,
      GLuint renderbuffer_service_id,
      GLenum error);
  void DoFramebufferTexture2D(
      GLenum target, GLenum attachment, GLenum tex_target,
      GLuint texture_client_id, GLuint texture_service_id,
      GLint level, GLenum error);
  GLenum DoCheckFramebufferStatus(GLenum target);
  void DoVertexAttribPointer(
      GLuint index, GLint size, GLenum type, GLsizei stride, GLuint offset);
  void DoVertexAttribDivisorANGLE(GLuint index, GLuint divisor);

  void DoEnableDisable(GLenum cap, bool enable);

  void SetDriverVertexAttribEnabled(GLint index, bool enable);
  void DoEnableVertexAttribArray(GLint index);

  void DoBufferData(GLenum target, GLsizei size);

  void DoBufferSubData(
      GLenum target, GLint offset, GLsizei size, const void* data);

  void DoScissor(GLint x, GLint y, GLsizei width, GLsizei height);

  void DoPixelStorei(GLenum pname, GLint param);

  void SetupVertexBuffer();
  void SetupAllNeededVertexBuffers();

  void SetupIndexBuffer();

  void DeleteVertexBuffer();

  void DeleteIndexBuffer();

  void SetupClearTextureExpectations(GLuint service_id,
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
                                     GLuint bound_pixel_unpack_buffer);

  void SetupClearTexture3DExpectations(GLsizeiptr buffer_size,
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
                                       GLuint bound_pixel_unpack_buffer);

  void SetupExpectationsForRestoreClearState(GLclampf restore_red,
                                             GLclampf restore_green,
                                             GLclampf restore_blue,
                                             GLclampf restore_alpha,
                                             GLuint restore_stencil,
                                             GLclampf restore_depth,
                                             bool restore_scissor_test,
                                             GLint restore_scissor_x,
                                             GLint restore_scissor_y,
                                             GLsizei restore_scissor_width,
                                             GLsizei restore_scissor_height);

  void SetupExpectationsForFramebufferClearing(GLenum target,
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
                                               GLsizei restore_scissor_height);

  void SetupExpectationsForFramebufferClearingMulti(
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
      GLsizei restore_scissor_height);

  void SetupExpectationsForDepthMask(bool mask);
  void SetupExpectationsForEnableDisable(GLenum cap, bool enable);
  void SetupExpectationsForColorMask(bool red,
                                     bool green,
                                     bool blue,
                                     bool alpha);
  void SetupExpectationsForStencilMask(GLuint front_mask, GLuint back_mask);

  void SetupExpectationsForApplyingDirtyState(
      bool framebuffer_is_rgb,
      bool framebuffer_has_depth,
      bool framebuffer_has_stencil,
      GLuint color_bits,  // NOTE! bits are 0x1000, 0x0100, 0x0010, and 0x0001
      bool depth_mask,
      bool depth_enabled,
      GLuint front_stencil_mask,
      GLuint back_stencil_mask,
      bool stencil_enabled);

  void SetupExpectationsForApplyingDefaultDirtyState();

  void AddExpectationsForGenVertexArraysOES();
  void AddExpectationsForDeleteVertexArraysOES();
  void AddExpectationsForDeleteBoundVertexArraysOES();
  void AddExpectationsForBindVertexArrayOES();
  void AddExpectationsForRestoreAttribState(GLuint attrib);

  void DoInitializeDiscardableTextureCHROMIUM(GLuint texture_id);
  void DoUnlockDiscardableTextureCHROMIUM(GLuint texture_id);
  void DoLockDiscardableTextureCHROMIUM(GLuint texture_id);
  bool IsDiscardableTextureUnlocked(GLuint texture_id);

  GLvoid* BufferOffset(unsigned i) { return reinterpret_cast<GLvoid*>(i); }

  template <typename Command, typename Result>
  bool IsObjectHelper(GLuint client_id) {
    Result* result = static_cast<Result*>(shared_memory_address_);
    Command cmd;
    cmd.Init(client_id, shared_memory_id_, kSharedMemoryOffset);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    bool isObject = static_cast<bool>(*result);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    return isObject;
  }

 protected:
  static const int kBackBufferWidth = 128;
  static const int kBackBufferHeight = 64;

  static const GLint kMaxTextureSize = 2048;
  static const GLint kMaxCubeMapTextureSize = 256;
  static const GLint kNumVertexAttribs = 16;
  static const GLint kNumTextureUnits = 8;
  static const GLint kMaxTextureImageUnits = 8;
  static const GLint kMaxVertexTextureImageUnits = 2;
  static const GLint kMaxFragmentUniformVectors = 16;
  static const GLint kMaxVaryingVectors = 8;
  static const GLint kMaxVertexUniformVectors = 128;
  static const GLint kMaxViewportWidth = 8192;
  static const GLint kMaxViewportHeight = 8192;

  static const GLuint kServiceAttrib0BufferId = 801;
  static const GLuint kServiceFixedAttribBufferId = 802;

  static const GLuint kServiceBufferId = 301;
  static const GLuint kServiceFramebufferId = 302;
  static const GLuint kServiceRenderbufferId = 303;
  static const GLuint kServiceTextureId = 304;
  static const GLuint kServiceProgramId = 305;
  static const GLuint kServiceSamplerId = 306;
  static const GLuint kServiceShaderId = 307;
  static const GLuint kServiceElementBufferId = 308;
  static const GLuint kServiceQueryId = 309;
  static const GLuint kServiceVertexArrayId = 310;
  static const GLuint kServiceTransformFeedbackId = 311;
  static const GLuint kServiceDefaultTransformFeedbackId = 312;
  static const GLuint kServiceSyncId = 313;

  static const size_t kSharedBufferSize = 2048;
  static const uint32_t kSharedMemoryOffset = 132;
  static const int32_t kInvalidSharedMemoryId =
      FakeCommandBufferServiceBase::kTransferBufferBaseId - 1;
  static const uint32_t kInvalidSharedMemoryOffset = kSharedBufferSize + 1;
  static const uint32_t kInitialResult = 0xBDBDBDBDu;
  static const uint8_t kInitialMemoryValue = 0xBDu;

  static const uint32_t kNewClientId = 501;
  static const uint32_t kNewServiceId = 502;
  static const uint32_t kInvalidClientId = 601;

  static const GLuint kServiceVertexShaderId = 321;
  static const GLuint kServiceFragmentShaderId = 322;

  static const GLuint kServiceCopyTextureChromiumShaderId = 701;
  static const GLuint kServiceCopyTextureChromiumProgramId = 721;

  static const GLuint kServiceCopyTextureChromiumTextureBufferId = 751;
  static const GLuint kServiceCopyTextureChromiumVertexBufferId = 752;
  static const GLuint kServiceCopyTextureChromiumFBOId = 753;
  static const GLuint kServiceCopyTextureChromiumPositionAttrib = 761;
  static const GLuint kServiceCopyTextureChromiumTexAttrib = 762;
  static const GLuint kServiceCopyTextureChromiumSamplerLocation = 763;

  static const GLsizei kNumVertices = 100;
  static const GLsizei kNumIndices = 10;
  static const int kValidIndexRangeStart = 1;
  static const int kValidIndexRangeCount = 7;
  static const int kInvalidIndexRangeStart = 0;
  static const int kInvalidIndexRangeCount = 7;
  static const int kOutOfRangeIndexRangeEnd = 10;
  static const GLuint kMaxValidIndex = 7;

  static const GLint kMaxAttribLength = 10;
  static const char* kAttrib1Name;
  static const char* kAttrib2Name;
  static const char* kAttrib3Name;
  static const GLint kAttrib1Size = 1;
  static const GLint kAttrib2Size = 1;
  static const GLint kAttrib3Size = 1;
  static const GLint kAttrib1Location = 0;
  static const GLint kAttrib2Location = 1;
  static const GLint kAttrib3Location = 2;
  static const GLenum kAttrib1Type = GL_FLOAT_VEC4;
  static const GLenum kAttrib2Type = GL_FLOAT_VEC2;
  static const GLenum kAttrib3Type = GL_FLOAT_VEC3;
  static const GLint kInvalidAttribLocation = 30;
  static const GLint kBadAttribIndex = kNumVertexAttribs;

  static const GLint kMaxUniformLength = 12;
  static const char* kUniform1Name;
  static const char* kUniform2Name;
  static const char* kUniform3Name;
  static const char* kUniform4Name;
  static const char* kUniform5Name;
  static const char* kUniform6Name;
  static const char* kUniform7Name;
  static const char* kUniform8Name;
  static const GLint kUniform1Size = 1;
  static const GLint kUniform2Size = 3;
  static const GLint kUniform3Size = 2;
  static const GLint kUniform4Size = 1;
  static const GLint kUniform5Size = 1;
  static const GLint kUniform6Size = 1;
  static const GLint kUniform7Size = 1;
  static const GLint kUniform8Size = 2;
  static const GLint kUniform1RealLocation = 3;
  static const GLint kUniform2RealLocation = 10;
  static const GLint kUniform2ElementRealLocation = 12;
  static const GLint kUniform3RealLocation = 20;
  static const GLint kUniform4RealLocation = 22;
  static const GLint kUniform5RealLocation = 30;
  static const GLint kUniform6RealLocation = 32;
  static const GLint kUniform7RealLocation = 44;
  static const GLint kUniform8RealLocation = 56;
  static const GLint kUniform1FakeLocation = 0;               // These are
  static const GLint kUniform2FakeLocation = 1;               // hardcoded
  static const GLint kUniform2ElementFakeLocation = 0x10001;  // to match
  static const GLint kUniform3FakeLocation = 2;               // ProgramManager.
  static const GLint kUniform4FakeLocation = 3;               //
  static const GLint kUniform5FakeLocation = 4;               //
  static const GLint kUniform6FakeLocation = 5;               //
  static const GLint kUniform7FakeLocation = 6;               //
  static const GLint kUniform8FakeLocation = 7;               //
  static const GLint kUniform1DesiredLocation = -1;
  static const GLint kUniform2DesiredLocation = -1;
  static const GLint kUniform3DesiredLocation = -1;
  static const GLint kUniform4DesiredLocation = -1;
  static const GLint kUniform5DesiredLocation = -1;
  static const GLint kUniform6DesiredLocation = -1;
  static const GLint kUniform7DesiredLocation = -1;
  static const GLint kUniform8DesiredLocation = -1;
  static const GLenum kUniform1Type = GL_SAMPLER_2D;
  static const GLenum kUniform2Type = GL_INT_VEC2;
  static const GLenum kUniform3Type = GL_FLOAT_VEC3;
  static const GLenum kUniform4Type = GL_UNSIGNED_INT;
  static const GLenum kUniform5Type = GL_UNSIGNED_INT_VEC2;
  static const GLenum kUniform6Type = GL_UNSIGNED_INT_VEC3;
  static const GLenum kUniform7Type = GL_UNSIGNED_INT_VEC4;
  static const GLenum kUniform8Type = GL_INT;
  static const GLenum kUniformSamplerExternalType = GL_SAMPLER_EXTERNAL_OES;
  static const GLenum kUniformCubemapType = GL_SAMPLER_CUBE;
  static const GLint kInvalidUniformLocation = 30;
  static const GLint kBadUniformIndex = 1000;

  static const GLint kOutputVariable1Size = 0;
  static const GLenum kOutputVariable1Type = GL_FLOAT_VEC4;
  static const GLuint kOutputVariable1ColorName = 7;
  static const GLuint kOutputVariable1Index = 0;
  static const char* kOutputVariable1Name;
  static const char* kOutputVariable1NameESSL3;

  // Use StrictMock to make 100% sure we know how GL will be called.
  std::unique_ptr<::testing::StrictMock<::gl::MockGLInterface>> gl_;
  scoped_refptr<gl::GLSurfaceStub> surface_;
  scoped_refptr<GLContextMock> context_;
  std::unique_ptr<FakeCommandBufferServiceBase> command_buffer_service_;
  TraceOutputter outputter_;
  std::unique_ptr<MockGLES2Decoder> mock_decoder_;
  std::unique_ptr<GLES2Decoder> decoder_;
  std::unique_ptr<MemoryTracker> memory_tracker_;
  raw_ptr<gl::GLDisplay> display_ = nullptr;

  GLuint client_buffer_id_;
  GLuint client_framebuffer_id_;
  GLuint client_program_id_;
  GLuint client_renderbuffer_id_;
  GLuint client_sampler_id_;
  GLuint client_shader_id_;
  GLuint client_texture_id_;
  GLuint client_element_buffer_id_;
  GLuint client_vertex_shader_id_;
  GLuint client_fragment_shader_id_;
  GLuint client_query_id_;
  GLuint client_vertexarray_id_;
  GLuint client_transformfeedback_id_;
  GLuint client_sync_id_;

  int32_t shared_memory_id_;
  uint32_t shared_memory_offset_;
  raw_ptr<void> shared_memory_address_;
  raw_ptr<void> shared_memory_base_;

  GLuint service_renderbuffer_id_;
  bool service_renderbuffer_valid_;

  uint32_t immediate_buffer_[64];

  const bool ignore_cached_state_for_test_;
  bool cached_color_mask_red_;
  bool cached_color_mask_green_;
  bool cached_color_mask_blue_;
  bool cached_color_mask_alpha_;
  bool cached_depth_mask_;
  GLuint cached_stencil_front_mask_;
  GLuint cached_stencil_back_mask_;

  struct EnableFlags {
    EnableFlags();
    bool cached_blend;
    bool cached_cull_face;
    bool cached_depth_test;
    bool cached_dither;
    bool cached_polygon_offset_fill;
    bool cached_sample_alpha_to_coverage;
    bool cached_sample_coverage;
    bool cached_scissor_test;
    bool cached_stencil_test;
  };

  EnableFlags enable_flags_;

  int shader_language_version_;

  std::array<bool, kNumVertexAttribs> attribs_enabled_ = {};

 private:
  // MockGLStates is used to track GL states and emulate driver
  // behaviors on top of MockGLInterface.
  class MockGLStates {
   public:
    MockGLStates()
        : bound_array_buffer_object_(0),
          bound_vertex_array_object_(0) {
    }

    ~MockGLStates() = default;

    void OnBindArrayBuffer(GLuint id) {
      bound_array_buffer_object_ = id;
    }

    void OnBindVertexArrayOES(GLuint id) {
      bound_vertex_array_object_ = id;
    }

    void OnVertexAttribNullPointer() {
      // When a vertex array object is bound, some drivers (AMD Linux,
      // Qualcomm, etc.) have a bug where it incorrectly generates an
      // GL_INVALID_OPERATION on glVertexAttribPointer() if pointer
      // is nullptr, no buffer is bound on GL_ARRAY_BUFFER.
      // Make sure we don't trigger this bug.
      if (bound_vertex_array_object_ != 0)
        EXPECT_TRUE(bound_array_buffer_object_ != 0);
    }

   private:
    GLuint bound_array_buffer_object_;
    GLuint bound_vertex_array_object_;
  };  // class MockGLStates

  void SetupMockGLBehaviors();

  GpuPreferences gpu_preferences_;
  ShaderTranslatorCache shader_translator_cache_;
  FramebufferCompletenessCache framebuffer_completeness_cache_;
  ServiceDiscardableManager discardable_manager_;
  SharedImageManager shared_image_manager_;
  scoped_refptr<ContextGroup> group_;
  MockGLStates gl_states_;
  base::test::SingleThreadTaskEnvironment task_environment_;

  raw_ptr<MockCopyTextureResourceManager, DanglingUntriaged>
      copy_texture_manager_;  // not owned
  raw_ptr<MockCopyTexImageResourceManager, DanglingUntriaged>
      copy_tex_image_blitter_;  // not owned
};

class GLES2DecoderWithShaderTestBase : public GLES2DecoderTestBase {
 public:
  GLES2DecoderWithShaderTestBase()
      : GLES2DecoderTestBase() {
  }

 protected:
  void SetUp() override;
  void TearDown() override;
};

// SpecializedSetup specializations that are needed in multiple unittest files.
template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::LinkProgram, 0>(bool valid);

MATCHER_P2(PointsToArray, array, size, "") {
  for (size_t i = 0; i < static_cast<size_t>(size); ++i) {
    if (arg[i] != array[i])
      return false;
  }
  return true;
}

class GLES2DecoderPassthroughTestBase : public testing::Test,
                                        public DecoderClient {
 public:
  GLES2DecoderPassthroughTestBase(ContextType context_type);
  ~GLES2DecoderPassthroughTestBase() override;

  void OnConsoleMessage(int32_t id, const std::string& message) override;
  void CacheBlob(gpu::GpuDiskCacheType type,
                 const std::string& key,
                 const std::string& blob) override;
  void OnFenceSyncRelease(uint64_t release) override;
  void OnDescheduleUntilFinished() override;
  void OnRescheduleAfterFinished() override;
  void OnSwapBuffers(uint64_t swap_id, uint32_t flags) override;
  void ScheduleGrContextCleanup() override {}
  void HandleReturnData(base::span<const uint8_t> data) override {}
  bool ShouldYield() override;

  void SetUp() override;
  void TearDown() override;

  template <typename T>
  void GenHelper(GLuint client_id) {
    int8_t buffer[sizeof(T) + sizeof(client_id)];
    T& cmd = *reinterpret_cast<T*>(&buffer);
    cmd.Init(1, &client_id);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(client_id)));
  }

  template <typename Command>
  bool IsObjectHelper(GLuint client_id) {
    typename Command::Result* result =
        static_cast<typename Command::Result*>(shared_memory_address_);
    Command cmd;
    cmd.Init(client_id, shared_memory_id_, kSharedMemoryOffset);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    bool isObject = static_cast<bool>(*result);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    return isObject;
  }

  template <typename T>
  error::Error ExecuteCmd(const T& cmd) {
    static_assert(T::kArgFlags == cmd::kFixed,
                  "T::kArgFlags should equal cmd::kFixed");
    int entries_processed = 0;
    return decoder_->DoCommands(1, (const void*)&cmd,
                                ComputeNumEntries(sizeof(cmd)),
                                &entries_processed);
  }

  template <typename T>
  error::Error ExecuteImmediateCmd(const T& cmd, size_t data_size) {
    static_assert(T::kArgFlags == cmd::kAtLeastN,
                  "T::kArgFlags should equal cmd::kAtLeastN");
    int entries_processed = 0;
    return decoder_->DoCommands(1, (const void*)&cmd,
                                ComputeNumEntries(sizeof(cmd) + data_size),
                                &entries_processed);
  }

  void SetBucketData(uint32_t bucket_id, const void* data, size_t data_size);

  template <typename T>
  T GetSharedMemoryAs() {
    return reinterpret_cast<T>(shared_memory_address_.get());
  }

  template <typename T>
  T GetSharedMemoryAsWithSize(size_t* out_shmem_size) {
    *out_shmem_size = shared_memory_size_;
    return reinterpret_cast<T>(shared_memory_address_.get());
  }

  template <typename T>
  T GetSharedMemoryAsWithOffset(uint32_t offset) {
    void* ptr =
        reinterpret_cast<int8_t*>(shared_memory_address_.get()) + offset;
    return reinterpret_cast<T>(ptr);
  }

  template <typename T>
  T GetSharedMemoryAsWithOffsetAndSize(uint32_t offset,
                                       size_t* out_shmem_size) {
    EXPECT_LT(offset, shared_memory_size_);
    *out_shmem_size = shared_memory_size_ - offset;
    void* ptr =
        reinterpret_cast<int8_t*>(shared_memory_address_.get()) + offset;
    return reinterpret_cast<T>(ptr);
  }

  template <typename T>
  T* GetImmediateAs() {
    return reinterpret_cast<T*>(immediate_buffer_);
  }

  GLES2DecoderPassthroughImpl* GetDecoder() const { return decoder_.get(); }
  PassthroughResources* GetPassthroughResources() const {
    return group_->passthrough_resources();
  }
  SharedImageRepresentationFactory* GetSharedImageRepresentationFactory()
      const {
    return group_->shared_image_representation_factory();
  }
  SharedImageManager* GetSharedImageManager() { return &shared_image_manager_; }
  const base::circular_deque<GLES2DecoderPassthroughImpl::PendingReadPixels>&
  GetPendingReadPixels() const {
    return decoder_->pending_read_pixels_;
  }

  GLint GetGLError();

 protected:
  void DoRequestExtension(const char* extension);

  void DoBindBuffer(GLenum target, GLuint client_id);
  void DoDeleteBuffer(GLuint client_id);
  void DoBufferData(GLenum target,
                    GLsizei size,
                    const void* data,
                    GLenum usage);
  void DoBufferSubData(GLenum target,
                       GLint offset,
                       GLsizeiptr size,
                       const void* data);

  void DoGenTexture(GLuint client_id);
  bool DoIsTexture(GLuint client_id);
  void DoBindTexture(GLenum target, GLuint client_id);
  void DoDeleteTexture(GLuint client_id);
  void DoTexImage2D(GLenum target,
                    GLint level,
                    GLenum internal_format,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    uint32_t shared_memory_id,
                    uint32_t shared_memory_offset);

  void DoBindFramebuffer(GLenum target, GLuint client_id);
  void DoFramebufferTexture2D(GLenum target,
                              GLenum attachment,
                              GLenum textarget,
                              GLuint texture_client_id,
                              GLint level);
  void DoFramebufferRenderbuffer(GLenum target,
                                 GLenum attachment,
                                 GLenum renderbuffertarget,
                                 GLuint renderbuffer);

  void DoBindRenderbuffer(GLenum target, GLuint client_id);

  void DoGetIntegerv(GLenum pname, GLint* result, size_t num_results);

  void DoInitializeDiscardableTextureCHROMIUM(GLuint client_id);
  void DoUnlockDiscardableTextureCHROMIUM(GLuint client_id);
  void DoLockDiscardableTextureCHROMIUM(GLuint client_id);

  PassthroughDiscardableManager* passthrough_discardable_texture_manager() {
    return &passthrough_discardable_manager_;
  }
  ContextGroup* group() { return group_.get(); }
  FeatureInfo* feature_info() { return group_->feature_info(); }

  static const size_t kSharedBufferSize = 2048;
  static const uint32_t kSharedMemoryOffset = 132;
  static const uint32_t kInvalidSharedMemoryOffset = kSharedBufferSize + 1;
  static const int32_t kInvalidSharedMemoryId =
      FakeCommandBufferServiceBase::kTransferBufferBaseId - 1;

  static const uint32_t kNewClientId = 501;
  static const GLuint kClientBufferId = 100;
  static const GLuint kClientTextureId = 101;
  static const GLuint kClientFramebufferId = 102;
  static const GLuint kClientRenderbufferId = 103;

  int32_t shared_memory_id_;
  uint32_t shared_memory_offset_;
  raw_ptr<void> shared_memory_address_;
  raw_ptr<void> shared_memory_base_;
  size_t shared_memory_size_;

  uint32_t immediate_buffer_[64];

 private:
  ContextCreationAttribs context_creation_attribs_;
  GpuPreferences gpu_preferences_;
  ShaderTranslatorCache shader_translator_cache_;
  FramebufferCompletenessCache framebuffer_completeness_cache_;
  ServiceDiscardableManager discardable_manager_;
  PassthroughDiscardableManager passthrough_discardable_manager_;
  SharedImageManager shared_image_manager_;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  std::unique_ptr<FakeCommandBufferServiceBase> command_buffer_service_;
  TraceOutputter outputter_;
  std::unique_ptr<GLES2DecoderPassthroughImpl> decoder_;
  scoped_refptr<ContextGroup> group_;
  raw_ptr<gl::GLDisplay> display_ = nullptr;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_BASE_H_
