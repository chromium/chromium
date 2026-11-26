// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>

#include "base/no_destructor.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"
#include "gpu/command_buffer/service/passthrough_program_cache.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {
namespace gles2 {

template <typename T>
class GLES2DecoderPassthroughFixedCommandTest
    : public GLES2DecoderPassthroughTest {};
TYPED_TEST_SUITE_P(GLES2DecoderPassthroughFixedCommandTest);

TYPED_TEST_P(GLES2DecoderPassthroughFixedCommandTest, InvalidCommand) {
  TypeParam cmd;
  cmd.SetHeader();
  EXPECT_EQ(error::kUnknownCommand, this->ExecuteCmd(cmd));
}
REGISTER_TYPED_TEST_SUITE_P(GLES2DecoderPassthroughFixedCommandTest,
                            InvalidCommand);

template <typename T>
class GLES2DecoderPassthroughImmediateNoArgCommandTest
    : public GLES2DecoderPassthroughTest {};
TYPED_TEST_SUITE_P(GLES2DecoderPassthroughImmediateNoArgCommandTest);

TYPED_TEST_P(GLES2DecoderPassthroughImmediateNoArgCommandTest, InvalidCommand) {
  auto& cmd = *(this->template GetImmediateAs<TypeParam>());
  cmd.SetHeader();
  EXPECT_EQ(error::kUnknownCommand, this->ExecuteImmediateCmd(cmd, 64));
}
REGISTER_TYPED_TEST_SUITE_P(GLES2DecoderPassthroughImmediateNoArgCommandTest,
                            InvalidCommand);

template <typename T>
class GLES2DecoderPassthroughImmediateSizeArgCommandTest
    : public GLES2DecoderPassthroughTest {};
TYPED_TEST_SUITE_P(GLES2DecoderPassthroughImmediateSizeArgCommandTest);

TYPED_TEST_P(GLES2DecoderPassthroughImmediateSizeArgCommandTest,
             InvalidCommand) {
  auto& cmd = *(this->template GetImmediateAs<TypeParam>());
  cmd.SetHeader(0);
  EXPECT_EQ(error::kUnknownCommand, this->ExecuteImmediateCmd(cmd, 0));
}
REGISTER_TYPED_TEST_SUITE_P(GLES2DecoderPassthroughImmediateSizeArgCommandTest,
                            InvalidCommand);

using ES3FixedCommandTypes0 =
    ::testing::Types<cmds::BindBufferBase,
                     cmds::BindBufferRange,
                     cmds::BindSampler,
                     cmds::BindTransformFeedback,
                     cmds::ClearBufferfi,
                     cmds::ClientWaitSync,
                     cmds::CopyBufferSubData,
                     cmds::CompressedTexImage3D,
                     cmds::CompressedTexImage3DBucket,
                     cmds::CompressedTexSubImage3D,
                     cmds::CompressedTexSubImage3DBucket,
                     cmds::CopyTexSubImage3D,
                     cmds::DeleteSync,
                     cmds::FenceSync,
                     cmds::FramebufferTextureLayer,
                     cmds::GetActiveUniformBlockiv,
                     cmds::GetActiveUniformBlockName,
                     cmds::GetActiveUniformsiv,
                     cmds::GetFragDataLocation,
                     cmds::GetBufferParameteri64v,
                     cmds::GetInteger64v,
                     cmds::GetInteger64i_v,
                     cmds::GetIntegeri_v,
                     cmds::GetInternalformativ,
                     cmds::GetSamplerParameterfv,
                     cmds::GetSamplerParameteriv,
                     cmds::GetSynciv,
                     cmds::GetUniformBlockIndex,
                     cmds::GetUniformBlocksCHROMIUM,
                     cmds::GetUniformsES3CHROMIUM,
                     cmds::GetTransformFeedbackVarying,
                     cmds::GetTransformFeedbackVaryingsCHROMIUM,
                     cmds::GetUniformuiv,
                     cmds::GetUniformIndices,
                     cmds::GetVertexAttribIiv,
                     cmds::GetVertexAttribIuiv,
                     cmds::IsSampler,
                     cmds::IsSync,
                     cmds::IsTransformFeedback,
                     cmds::PauseTransformFeedback,
                     cmds::ReadBuffer,
                     cmds::ResumeTransformFeedback,
                     cmds::SamplerParameterf,
                     cmds::SamplerParameteri,
                     cmds::TexImage3D,
                     cmds::TexStorage3D,
                     cmds::TexSubImage3D>;

using ES3FixedCommandTypes1 =
    ::testing::Types<cmds::TransformFeedbackVaryingsBucket,
                     cmds::Uniform1ui,
                     cmds::Uniform2ui,
                     cmds::Uniform3ui,
                     cmds::Uniform4ui,
                     cmds::UniformBlockBinding,
                     cmds::VertexAttribI4i,
                     cmds::VertexAttribI4ui,
                     cmds::VertexAttribIPointer,
                     cmds::WaitSync,
                     cmds::BeginTransformFeedback,
                     cmds::EndTransformFeedback>;

using ES3ImmediateNoArgCommandTypes0 =
    ::testing::Types<cmds::ClearBufferivImmediate,
                     cmds::ClearBufferuivImmediate,
                     cmds::ClearBufferfvImmediate,
                     cmds::SamplerParameterfvImmediate,
                     cmds::SamplerParameterfvImmediate,
                     cmds::VertexAttribI4ivImmediate,
                     cmds::VertexAttribI4uivImmediate>;

using ES3ImmediateSizeArgCommandTypes0 =
    ::testing::Types<cmds::DeleteSamplersImmediate,
                     cmds::DeleteTransformFeedbacksImmediate,
                     cmds::GenTransformFeedbacksImmediate,
                     cmds::InvalidateFramebufferImmediate,
                     cmds::InvalidateSubFramebufferImmediate,
                     cmds::Uniform1uivImmediate,
                     cmds::Uniform2uivImmediate,
                     cmds::Uniform3uivImmediate,
                     cmds::Uniform4uivImmediate,
                     cmds::UniformMatrix2x3fvImmediate,
                     cmds::UniformMatrix2x4fvImmediate,
                     cmds::UniformMatrix3x2fvImmediate,
                     cmds::UniformMatrix3x4fvImmediate,
                     cmds::UniformMatrix4x2fvImmediate,
                     cmds::UniformMatrix4x3fvImmediate>;

INSTANTIATE_TYPED_TEST_SUITE_P(0,
                               GLES2DecoderPassthroughFixedCommandTest,
                               ES3FixedCommandTypes0);
INSTANTIATE_TYPED_TEST_SUITE_P(1,
                               GLES2DecoderPassthroughFixedCommandTest,
                               ES3FixedCommandTypes1);
INSTANTIATE_TYPED_TEST_SUITE_P(0,
                               GLES2DecoderPassthroughImmediateNoArgCommandTest,
                               ES3ImmediateNoArgCommandTypes0);
INSTANTIATE_TYPED_TEST_SUITE_P(
    0,
    GLES2DecoderPassthroughImmediateSizeArgCommandTest,
    ES3ImmediateSizeArgCommandTypes0);

// GL_TEXTURE_RECTANGLE_ANGLE is only for internal use and should not be
// reachable through the command buffer.
TEST_F(GLES2WebGLDecoderPassthroughTest, EnableDisableTextureRectangle) {
  {
    cmds::Enable cmd;
    cmd.Init(GL_TEXTURE_RECTANGLE_ANGLE);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  }
  {
    cmds::Disable cmd;
    cmd.Init(GL_TEXTURE_RECTANGLE_ANGLE);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  }
}

TEST_F(GLES2WebGLDecoderPassthroughTest, ContextVisibilityHintCHROMIUM) {
  {
    cmds::ContextVisibilityHintCHROMIUM cmd;
    cmd.Init(GL_FALSE);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }
  {
    cmds::ContextVisibilityHintCHROMIUM cmd;
    cmd.Init(GL_TRUE);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }
}

class GLES2WebGLDecoderPassthroughIdleTrimBaseTest
    : public GLES2WebGLDecoderPassthroughTest {
 public:
  void SetUp() override {
    GLES2WebGLDecoderPassthroughTest::SetUp();
    trim_count_ = 0;
    last_trim_level_ = 0;
    prev_get_string_fn_ = gl::g_current_gl_driver->fn.glGetStringFn;
    gl::g_current_gl_driver->fn.glGetStringFn =
        &GLES2WebGLDecoderPassthroughIdleTrimBaseTest::OnGetString;
    prev_trim_memory_fn_ = gl::g_current_gl_driver->fn.glTrimMemoryANGLEFn;
    gl::g_current_gl_driver->fn.glTrimMemoryANGLEFn =
        &GLES2WebGLDecoderPassthroughIdleTrimBaseTest::OnTrimMemoryANGLE;
    GetDecoder()->ForceReinitializeFeatureInfoForTesting();
  }

  void TearDown() override {
    gl::g_current_gl_driver->fn.glGetStringFn = prev_get_string_fn_;
    gl::g_current_gl_driver->fn.glTrimMemoryANGLEFn = prev_trim_memory_fn_;
    prev_get_string_fn_ = nullptr;
    prev_trim_memory_fn_ = nullptr;
    GLES2WebGLDecoderPassthroughTest::TearDown();
  }

 protected:
  static const GLubyte* GL_BINDING_CALL OnGetString(GLenum name) {
    const GLubyte* str = prev_get_string_fn_(name);
    if (name == GL_EXTENSIONS) {
      static base::NoDestructor<std::string> extensions;
      *extensions = str ? reinterpret_cast<const char*>(str) : "";
      *extensions += " GL_ANGLE_trim_memory";
      return reinterpret_cast<const GLubyte*>(extensions->c_str());
    }
    return str;
  }

  static void GL_BINDING_CALL OnTrimMemoryANGLE(GLenum trim_level) {
    trim_count_++;
    last_trim_level_ = trim_level;
  }

  inline static gl::glGetStringProc prev_get_string_fn_ = nullptr;
  inline static int trim_count_ = 0;
  inline static GLenum last_trim_level_ = 0;

 private:
  gl::glTrimMemoryANGLEProc prev_trim_memory_fn_ = nullptr;
};

class GLES2WebGLDecoderPassthroughIdleTrimTest
    : public GLES2WebGLDecoderPassthroughIdleTrimBaseTest {
 public:
  GLES2WebGLDecoderPassthroughIdleTrimTest() {
    feature_list_.InitAndEnableFeature(features::kANGLETrimMemoryOnIdle);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(GLES2WebGLDecoderPassthroughIdleTrimBaseTest,
       IdleTrimDisabledByDefault) {
  cmds::Finish cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  task_environment_.FastForwardBy(kIdleTrimDelay);
  EXPECT_EQ(0, trim_count_);
}

TEST_F(GLES2WebGLDecoderPassthroughIdleTrimTest,
       IdleTrimsMemoryAfterOneSecond) {
  cmds::Finish cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, trim_count_);

  task_environment_.FastForwardBy(kIdleTrimDelay / 2);
  EXPECT_EQ(0, trim_count_);

  task_environment_.FastForwardBy(kIdleTrimDelay / 2);
  EXPECT_EQ(1, trim_count_);
  EXPECT_EQ(static_cast<GLenum>(GL_MEMORY_TRIM_HIGH_ANGLE), last_trim_level_);

  task_environment_.FastForwardBy(5 * kIdleTrimDelay);
  EXPECT_EQ(1, trim_count_);
}

TEST_F(GLES2WebGLDecoderPassthroughIdleTrimTest, ActivityReschedulesIdleTrim) {
  cmds::Finish cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  task_environment_.FastForwardBy(base::Milliseconds(600));
  EXPECT_EQ(0, trim_count_);

  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  task_environment_.FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(0, trim_count_);

  task_environment_.FastForwardBy(base::Milliseconds(899));
  EXPECT_EQ(0, trim_count_);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(1, trim_count_);
  EXPECT_EQ(static_cast<GLenum>(GL_MEMORY_TRIM_HIGH_ANGLE), last_trim_level_);
}

TEST_F(GLES2WebGLDecoderPassthroughIdleTrimTest,
       MultipleCommandsInSameFrameTrimAfterOneSecond) {
  cmds::Finish cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  task_environment_.FastForwardBy(kIdleTrimDelay - base::Milliseconds(1));
  EXPECT_EQ(0, trim_count_);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(1, trim_count_);
}

TEST_F(GLES2WebGLDecoderPassthroughIdleTrimTest, ContextLossCancelsIdleTrim) {
  cmds::Finish cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  GetDecoder()->MarkContextLost(error::kUnknown);

  task_environment_.FastForwardBy(kIdleTrimDelay);
  EXPECT_EQ(0, trim_count_);
}

class GLES2WebGLDecoderPassthroughProgramCompletionTest
    : public GLES2WebGLDecoderPassthroughTest {
 public:
  void SetUp() override {
    GLES2WebGLDecoderPassthroughTest::SetUp();
    prev_get_programiv_fn_ = gl::g_current_gl_driver->fn.glGetProgramivFn;
    gl::g_current_gl_driver->fn.glGetProgramivFn =
        &GLES2WebGLDecoderPassthroughProgramCompletionTest::OnGetProgramiv;
  }

  void TearDown() override {
    gl::g_current_gl_driver->fn.glGetProgramivFn = prev_get_programiv_fn_;
    prev_get_programiv_fn_ = nullptr;
    GLES2WebGLDecoderPassthroughTest::TearDown();
  }

 protected:
  static void GL_BINDING_CALL OnGetProgramiv(GLuint program,
                                             GLenum pname,
                                             GLint* params) {
    if (pname == GL_COMPLETION_STATUS_KHR) {
      *params = GL_TRUE;
      return;
    }
    prev_get_programiv_fn_(program, pname, params);
  }

  void HandleReturnData(base::span<const uint8_t> data) override {
    ASSERT_GE(data.size(), sizeof(cmds::GLES2ReturnProgramInfo));
    cmds::GLES2ReturnProgramInfo return_info;
    base::byte_span_from_ref(return_info)
        .copy_from(data.first<sizeof(return_info)>());
    returned_program_infos_.push_back(return_info);
  }

  inline static gl::glGetProgramivProc prev_get_programiv_fn_ = nullptr;
  std::vector<cmds::GLES2ReturnProgramInfo> returned_program_infos_;
};

TEST_F(GLES2WebGLDecoderPassthroughProgramCompletionTest,
       ProgramCompletionQueryRelinkSkipsStaleReturnData) {
  constexpr GLuint kProgramClientId = 101;
  constexpr GLuint kQuery1ClientId = 201;
  constexpr GLuint kQuery2ClientId = 202;

  {
    cmds::CreateProgram cmd;
    cmd.Init(kProgramClientId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
  GenHelper<cmds::GenQueriesEXTImmediate>(kQuery1ClientId);
  GenHelper<cmds::GenQueriesEXTImmediate>(kQuery2ClientId);

  struct TwoQuerySyncs {
    QuerySync sync1;
    QuerySync sync2;
  };
  TwoQuerySyncs* syncs = GetSharedMemoryAs<TwoQuerySyncs*>();
  QuerySync* sync1 = &syncs->sync1;
  QuerySync* sync2 = &syncs->sync2;
  sync1->Reset();
  sync2->Reset();

  // Link 1 wrapped in Query 1.
  {
    cmds::BeginQueryEXT cmd;
    cmd.Init(GL_PROGRAM_COMPLETION_QUERY_CHROMIUM, kQuery1ClientId,
             shared_memory_id_, kSharedMemoryOffset);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
  {
    cmds::LinkProgram cmd;
    cmd.Init(kProgramClientId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
  {
    cmds::EndQueryEXT cmd;
    cmd.Init(GL_PROGRAM_COMPLETION_QUERY_CHROMIUM, 1);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  // Relink (Link 2) wrapped in Query 2 before Query 1 is processed.
  {
    cmds::BeginQueryEXT cmd;
    cmd.Init(GL_PROGRAM_COMPLETION_QUERY_CHROMIUM, kQuery2ClientId,
             shared_memory_id_, kSharedMemoryOffset + sizeof(QuerySync));
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
  {
    cmds::LinkProgram cmd;
    cmd.Init(kProgramClientId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
  {
    cmds::EndQueryEXT cmd;
    cmd.Init(GL_PROGRAM_COMPLETION_QUERY_CHROMIUM, 1);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  GetDecoder()->ProcessPendingQueries(false);

  EXPECT_EQ(1, base::subtle::Acquire_Load(&sync1->process_count));
  EXPECT_EQ(1, base::subtle::Acquire_Load(&sync2->process_count));

  // Only Link 2 (link_count == 2) should emit program info return data.
  ASSERT_EQ(1u, returned_program_infos_.size());
  EXPECT_EQ(GLES2ReturnDataType::kES2ProgramInfo,
            returned_program_infos_[0].return_data_header.return_data_type);
  EXPECT_EQ(kProgramClientId, returned_program_infos_[0].program_client_id);
  EXPECT_EQ(2u, returned_program_infos_[0].link_count);
}

class GLES2WebGLDecoderPassthroughCallbackTest
    : public GLES2WebGLDecoderPassthroughTest {
 public:
  GLES2WebGLDecoderPassthroughCallbackTest() {
    feature_list_.InitAndEnableFeature(features::kANGLEPerContextBlobCache);
  }

  void SetUp() override {
    // ANGLE's Null backend disables `cacheCompiledShader` by default, leaving
    // `MemoryShaderCache` null. Enable it so compiling shaders (including
    // pending compiles resolved during Context::onDestroy) exercises ANGLE's
    // per-context blob-cache callbacks.
    base::CommandLine* command_line =
        scoped_command_line_.GetProcessCommandLine();
    std::string enabled_angle_features =
        command_line->GetSwitchValueASCII(switches::kEnableANGLEFeatures);
    if (!enabled_angle_features.empty()) {
      enabled_angle_features += ",";
    }
    enabled_angle_features += "cacheCompiledShader";
    command_line->RemoveSwitch(switches::kEnableANGLEFeatures);
    command_line->AppendSwitchASCII(switches::kEnableANGLEFeatures,
                                    enabled_angle_features);
    GLES2WebGLDecoderPassthroughTest::SetUp();
  }

  void OnConsoleMessage(int32_t id, const std::string& message) override {
    ++console_message_count_;
    GLES2WebGLDecoderPassthroughTest::OnConsoleMessage(id, message);
  }

  void CacheBlob(gpu::GpuDiskCacheType type,
                 const std::string& key,
                 const std::string& blob) override {
    ++cached_blob_count_;
    GLES2WebGLDecoderPassthroughTest::CacheBlob(type, key, blob);
  }

 protected:
  struct SecondaryDecoderState {
    SecondaryDecoderState() {
      preferences.use_passthrough_cmd_decoder = true;
      translator_cache = std::make_unique<ShaderTranslatorCache>(preferences);
    }
    ~SecondaryDecoderState() = default;

    void DestroyDecoderWithoutContext() {
      decoder->Destroy(/*have_context=*/false);
      group->Destroy(decoder.get(), /*have_context=*/false);
      decoder.reset();
      group = nullptr;
    }

    PassthroughProgramCache program_cache{
        /*max_cache_size_bytes=*/1024 * 1024,
        /*disable_gpu_shader_disk_cache=*/false};
    GpuPreferences preferences;
    std::unique_ptr<ShaderTranslatorCache> translator_cache;
    FramebufferCompletenessCache completeness_cache;
    scoped_refptr<FeatureInfo> feature_info;
    scoped_refptr<ContextGroup> group;
    scoped_refptr<gl::GLSurface> surface;
    scoped_refptr<gl::GLContext> context;
    FakeCommandBufferServiceBase cb_service;
    TraceOutputter outputter;
    std::unique_ptr<GLES2DecoderPassthroughImpl> decoder;
  };

  void InitSecondaryDecoder(SecondaryDecoderState& state) {
    state.feature_info = base::MakeRefCounted<FeatureInfo>(
        workarounds_for_test_, GpuFeatureInfo());
    state.group = base::MakeRefCounted<ContextGroup>(
        state.preferences, /*memory_tracker=*/nullptr,
        state.translator_cache.get(), &state.completeness_cache,
        state.feature_info, /*progress_reporter=*/nullptr, GpuFeatureInfo(),
        GetSharedImageManager());
    state.group->set_program_cache(&state.program_cache);

    gl::GLDisplay* display = GetDecoder()->GetGLSurface()->GetGLDisplay();
    state.surface =
        gl::init::CreateOffscreenGLSurface(display, gfx::Size(4, 4));
    ASSERT_TRUE(state.surface);
    state.context = gl::init::CreateGLContext(
        nullptr, state.surface.get(),
        GenerateGLContextAttribsForDecoder(CONTEXT_TYPE_WEBGL1,
                                           gl::GpuPreference::kLowPower,
                                           state.group.get()));
    ASSERT_TRUE(state.context);
    ASSERT_TRUE(state.context->MakeCurrent(state.surface.get()));

    state.decoder = std::make_unique<GLES2DecoderPassthroughImpl>(
        this, &state.cb_service, &state.outputter, state.group.get());
    ASSERT_EQ(state.group->Initialize(state.decoder.get(), CONTEXT_TYPE_WEBGL1),
              gpu::ContextResult::kSuccess);
    ASSERT_EQ(state.decoder->Initialize(
                  state.surface, state.context, state.surface->IsSurfaceless(),
                  CONTEXT_TYPE_WEBGL1,
                  /*lose_context_when_out_of_memory=*/true),
              gpu::ContextResult::kSuccess);
  }

  int console_message_count_ = 0;
  int cached_blob_count_ = 0;

 private:
  base::test::ScopedCommandLine scoped_command_line_;
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/590804902): Compiling shaders via ANGLE and tearing down the
// GL display clears ANGLE's PoolIndex TLS and crashes subsequent
// ShaderTranslatorOutputVersionTest cases in the same gpu_unittests batch on
// Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_CallbacksDeliveredWhileContextActive \
  DISABLED_CallbacksDeliveredWhileContextActive
#define MAYBE_CallbacksIgnoredAfterContextLostInANGLE \
  DISABLED_CallbacksIgnoredAfterContextLostInANGLE
#define MAYBE_CallbacksIgnoredAfterContextLostWhileNotCurrent \
  DISABLED_CallbacksIgnoredAfterContextLostWhileNotCurrent
#else
#define MAYBE_CallbacksDeliveredWhileContextActive \
  CallbacksDeliveredWhileContextActive
#define MAYBE_CallbacksIgnoredAfterContextLostInANGLE \
  CallbacksIgnoredAfterContextLostInANGLE
#define MAYBE_CallbacksIgnoredAfterContextLostWhileNotCurrent \
  CallbacksIgnoredAfterContextLostWhileNotCurrent
#endif

TEST_F(GLES2WebGLDecoderPassthroughCallbackTest,
       MAYBE_CallbacksDeliveredWhileContextActive) {
  PassthroughProgramCache program_cache(
      /*max_cache_size_bytes=*/1024 * 1024,
      /*disable_gpu_shader_disk_cache=*/false);
  group()->set_program_cache(&program_cache);

  const char kKey[] = "sample_key";
  const char kValue[] = "sample_value";
  void* set_fn_ptr = nullptr;
  void* get_fn_ptr = nullptr;
  void* user_param = nullptr;
  gl::g_current_gl_context->glGetPointervKHRFn(GL_BLOB_CACHE_SET_FUNCTION_ANGLE,
                                               &set_fn_ptr);
  gl::g_current_gl_context->glGetPointervKHRFn(GL_BLOB_CACHE_GET_FUNCTION_ANGLE,
                                               &get_fn_ptr);
  gl::g_current_gl_context->glGetPointervKHRFn(GL_BLOB_CACHE_USER_PARAM_ANGLE,
                                               &user_param);
  ASSERT_NE(set_fn_ptr, nullptr);
  ASSERT_NE(get_fn_ptr, nullptr);
  ASSERT_NE(user_param, nullptr);

  auto set_fn = reinterpret_cast<GLSETBLOBPROCANGLE>(set_fn_ptr);
  auto get_fn = reinterpret_cast<GLGETBLOBPROCANGLE>(get_fn_ptr);
  set_fn(kKey, sizeof(kKey), kValue, sizeof(kValue), user_param);
  EXPECT_EQ(1, cached_blob_count_);
  char buffer[sizeof(kValue)] = {};
  EXPECT_EQ(static_cast<GLsizeiptr>(sizeof(kValue)),
            get_fn(kKey, sizeof(kKey), buffer, sizeof(buffer), user_param));

  // Verify that compiling a shader through ANGLE actually invokes the
  // per-context blob-cache callback (confirming `cacheCompiledShader` is
  // active on the Null backend).
  cached_blob_count_ = 0;
  GLuint shader = gl::g_current_gl_context->glCreateShaderFn(GL_VERTEX_SHADER);
  const char* kShaderSrc = "void main() { gl_Position = vec4(0.5); }";
  gl::g_current_gl_context->glShaderSourceFn(shader, 1, &kShaderSrc, nullptr);
  gl::g_current_gl_context->glCompileShaderFn(shader);
  GLint compile_status = GL_FALSE;
  gl::g_current_gl_context->glGetShaderivFn(shader, GL_COMPILE_STATUS,
                                            &compile_status);
  EXPECT_EQ(GL_TRUE, compile_status);
  EXPECT_GT(cached_blob_count_, 0);
  gl::g_current_gl_context->glDeleteShaderFn(shader);

  // Issue a capability not filtered by IsIgnoredCap() so ANGLE validates it
  // and dispatches GL_KHR_debug through PassthroughGLDebugMessageCallback().
  console_message_count_ = 0;
  cmds::Enable invalid_cmd;
  invalid_cmd.Init(GL_INVALID_ENUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(invalid_cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  EXPECT_GT(console_message_count_, 0);

  group()->set_program_cache(nullptr);
}

TEST_F(GLES2WebGLDecoderPassthroughCallbackTest,
       MAYBE_CallbacksIgnoredAfterContextLostInANGLE) {
  SecondaryDecoderState secondary;
  ASSERT_NO_FATAL_FAILURE(InitSecondaryDecoder(secondary));

  void* debug_fn_ptr = nullptr;
  void* debug_user_param = nullptr;
  void* blob_set_fn_ptr = nullptr;
  void* blob_get_fn_ptr = nullptr;
  void* blob_user_param = nullptr;
  gl::g_current_gl_context->glGetPointervKHRFn(GL_DEBUG_CALLBACK_FUNCTION,
                                               &debug_fn_ptr);
  gl::g_current_gl_context->glGetPointervKHRFn(GL_DEBUG_CALLBACK_USER_PARAM,
                                               &debug_user_param);
  gl::g_current_gl_context->glGetPointervKHRFn(GL_BLOB_CACHE_SET_FUNCTION_ANGLE,
                                               &blob_set_fn_ptr);
  gl::g_current_gl_context->glGetPointervKHRFn(GL_BLOB_CACHE_GET_FUNCTION_ANGLE,
                                               &blob_get_fn_ptr);
  gl::g_current_gl_context->glGetPointervKHRFn(GL_BLOB_CACHE_USER_PARAM_ANGLE,
                                               &blob_user_param);
  ASSERT_NE(debug_fn_ptr, nullptr);
  ASSERT_NE(debug_user_param, nullptr);
  ASSERT_NE(blob_set_fn_ptr, nullptr);
  ASSERT_NE(blob_get_fn_ptr, nullptr);
  ASSERT_NE(blob_user_param, nullptr);

  // Queue a shader compilation without querying GL_COMPILE_STATUS so ANGLE
  // resolves it during Context::onDestroy.
  GLuint shader = gl::g_current_gl_context->glCreateShaderFn(GL_VERTEX_SHADER);
  const char* kShaderSrc = "void main() { gl_Position = vec4(0.0); }";
  gl::g_current_gl_context->glShaderSourceFn(shader, 1, &kShaderSrc, nullptr);
  gl::g_current_gl_context->glCompileShaderFn(shader);

  // Mark the context lost inside ANGLE while current so ANGLE nulls its valid
  // context slot and drops subsequent GL callback clear calls.
  gl::g_current_gl_context->glRequestExtensionANGLEFn(
      "GL_CHROMIUM_lose_context");
  using LoseContextProc = void(GL_BINDING_CALL*)(GLenum, GLenum);
  auto lose_context_fn = reinterpret_cast<LoseContextProc>(
      gl::GetGLProcAddress("glLoseContextCHROMIUM"));
  ASSERT_NE(lose_context_fn, nullptr);
  lose_context_fn(GL_GUILTY_CONTEXT_RESET_KHR, GL_INNOCENT_CONTEXT_RESET_KHR);
  secondary.decoder->MarkContextLost(error::kGuilty);
  EXPECT_TRUE(secondary.decoder->WasContextLost());
  EXPECT_EQ(static_cast<GLenum>(GL_CONTEXT_LOST_KHR),
            gl::g_current_gl_context->glGetErrorFn());

  secondary.DestroyDecoderWithoutContext();

  // Trigger a GL error on the lost context and invoke the callbacks directly
  // after the decoder has been freed while `secondary.context` remains alive.
  console_message_count_ = 0;
  cached_blob_count_ = 0;
  gl::g_current_gl_context->glBindTextureFn(GL_TEXTURE_2D, 0);

  const char kMsg[] = "sample debug message";
  reinterpret_cast<GLDEBUGPROC>(debug_fn_ptr)(
      GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, 1, GL_DEBUG_SEVERITY_HIGH,
      sizeof(kMsg) - 1, kMsg, debug_user_param);
  const char kKey[] = "sample_key";
  const char kValue[] = "sample_value";
  reinterpret_cast<GLSETBLOBPROCANGLE>(blob_set_fn_ptr)(
      kKey, sizeof(kKey), kValue, sizeof(kValue), blob_user_param);
  EXPECT_EQ(0, reinterpret_cast<GLGETBLOBPROCANGLE>(blob_get_fn_ptr)(
                   kKey, sizeof(kKey), nullptr, 0, blob_user_param));
  EXPECT_EQ(0, console_message_count_);
  EXPECT_EQ(0, cached_blob_count_);

  // Dropping the last reference to `secondary.context` runs Context::onDestroy,
  // which resolves the pending shader compile and invokes the blob cache
  // callback in ANGLE.
  secondary.context->ReleaseCurrent(secondary.surface.get());
  secondary.surface = nullptr;
  secondary.context = nullptr;
  EXPECT_EQ(0, console_message_count_);
  EXPECT_EQ(0, cached_blob_count_);

  ASSERT_TRUE(GetDecoder()->MakeCurrent());
}

TEST_F(GLES2WebGLDecoderPassthroughCallbackTest,
       MAYBE_CallbacksIgnoredAfterContextLostWhileNotCurrent) {
  SecondaryDecoderState secondary;
  ASSERT_NO_FATAL_FAILURE(InitSecondaryDecoder(secondary));

  GLuint shader = gl::g_current_gl_context->glCreateShaderFn(GL_VERTEX_SHADER);
  const char* kShaderSrc = "void main() { gl_Position = vec4(1.0); }";
  gl::g_current_gl_context->glShaderSourceFn(shader, 1, &kShaderSrc, nullptr);
  gl::g_current_gl_context->glCompileShaderFn(shader);

  // Switch back to the primary context and create two textures on the primary
  // decoder: one retained only in `texture_object_map` (unbound) and one
  // retained only in `bound_textures_` (removed from `texture_object_map`), so
  // each branch of MarkContextLost() is tested independently when the last
  // TexturePassthrough reference is dropped below.
  ASSERT_TRUE(GetDecoder()->MakeCurrent());
  EXPECT_FALSE(secondary.context->IsCurrent(nullptr));

  DoGenTexture(kClientTextureId);
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);
  scoped_refptr<TexturePassthrough> resource_texture;
  ASSERT_TRUE(GetPassthroughResources()->texture_object_map.GetServiceID(
      kClientTextureId, &resource_texture));
  ASSERT_TRUE(resource_texture);
  const GLuint resource_service_id = resource_texture->service_id();
  DoBindTexture(GL_TEXTURE_2D, 0);

  constexpr GLuint kBoundClientTextureId = kClientTextureId + 1;
  DoGenTexture(kBoundClientTextureId);
  DoBindTexture(GL_TEXTURE_2D, kBoundClientTextureId);
  scoped_refptr<TexturePassthrough> bound_texture;
  ASSERT_TRUE(GetPassthroughResources()->texture_object_map.GetServiceID(
      kBoundClientTextureId, &bound_texture));
  ASSERT_TRUE(bound_texture);
  const GLuint bound_service_id = bound_texture->service_id();
  GetPassthroughResources()->texture_object_map.RemoveClientID(
      kBoundClientTextureId);
  GetPassthroughResources()->texture_id_map.RemoveClientID(
      kBoundClientTextureId);

  // Lose and destroy `secondary.decoder` while `secondary.context` is not
  // current, so neither MarkContextLost() nor Destroy(false) can issue GL calls
  // on `secondary.context`.
  secondary.decoder->MarkContextLost(error::kUnknown);
  secondary.DestroyDecoderWithoutContext();

  // Make `secondary.context` current again so the primary context is not
  // current when the primary decoder's MarkContextLost() runs.
  ASSERT_TRUE(secondary.context->MakeCurrent(secondary.surface.get()));
  GetDecoder()->MarkContextLost(error::kUnknown);
  EXPECT_TRUE(GetDecoder()->WasContextLost());

  // Trigger a GL debug error callback and destroy `secondary.context` to
  // resolve the pending shader compilation.
  console_message_count_ = 0;
  cached_blob_count_ = 0;
  gl::g_current_gl_context->glBindTextureFn(GL_INVALID_ENUM, 0);
  secondary.context->ReleaseCurrent(secondary.surface.get());
  secondary.surface = nullptr;
  secondary.context = nullptr;
  EXPECT_EQ(0, console_message_count_);
  EXPECT_EQ(0, cached_blob_count_);

  ASSERT_TRUE(
      GetDecoder()->GetGLContext()->MakeCurrent(GetDecoder()->GetGLSurface()));

  // Drop the remaining map reference and local reference to `resource_texture`
  // so ~TexturePassthrough() runs now; it must not delete `resource_service_id`
  // because resources_->MarkContextLost() ran while non-current.
  GetPassthroughResources()->texture_object_map.RemoveClientID(
      kClientTextureId);
  GetPassthroughResources()->texture_id_map.RemoveClientID(kClientTextureId);
  ASSERT_TRUE(resource_texture->HasOneRef());
  resource_texture.reset();
  EXPECT_EQ(static_cast<GLboolean>(GL_TRUE),
            gl::g_current_gl_context->glIsTextureFn(resource_service_id));

  // Unbind on the primary decoder to drop `bound_textures_`'s reference, then
  // drop `bound_texture` so ~TexturePassthrough() runs now; it must not delete
  // `bound_service_id` because MarkContextLost() marked `bound_textures_` lost
  // while non-current.
  cmds::BindTexture unbind_cmd;
  unbind_cmd.Init(GL_TEXTURE_2D, 0);
  EXPECT_EQ(error::kLostContext, ExecuteCmd(unbind_cmd));
  ASSERT_TRUE(bound_texture->HasOneRef());
  bound_texture.reset();
  EXPECT_EQ(static_cast<GLboolean>(GL_TRUE),
            gl::g_current_gl_context->glIsTextureFn(bound_service_id));

  gl::g_current_gl_context->glDeleteTexturesFn(1, &resource_service_id);
  gl::g_current_gl_context->glDeleteTexturesFn(1, &bound_service_id);
}

}  // namespace gles2
}  // namespace gpu
