// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>

#include "base/command_line.h"
#include "base/containers/heap_array.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_direct.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/passthrough_discardable_manager.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {
namespace {

constexpr int kDefaultRuns = 8;
constexpr int kDefaultIterations = 50000;

// A command buffer that can record and replay commands
// This goes through 3 states, allowing setting up of initial state before
// record/replay a tight loop:
// - kDirect directly sends commands to the service on Flush
// - kRecord doesn't send anything on Flush, but keeps track of the put pointer
// - kReplay allows replaying of commands recorded in the kRecord state
//
// The initial state is kDirect. AdvanceMode is used to transition from one
// state to the next. The transition from kDirect to kRecord requires the
// GetBuffer to have been freed, allowing a fresh start for record.
class RecordReplayCommandBuffer : public CommandBufferDirect {
 public:
  enum Mode { kDirect, kRecord, kReplay };

  RecordReplayCommandBuffer() = default;
  ~RecordReplayCommandBuffer() override = default;

  void AdvanceMode() {
    switch (mode_) {
      case kDirect:
        mode_ = kRecord;
        DCHECK_EQ(current_get_buffer_, -1);
        DCHECK_EQ(service()->GetState().get_offset, 0);
        break;
      case kRecord:
        mode_ = kReplay;
        DCHECK_NE(saved_get_buffer_, -1);
        CommandBufferDirect::SetGetBuffer(saved_get_buffer_);
        break;
      case kReplay:
        mode_ = kDirect;
        break;
    }
  }

  void Flush(int32_t put_offset) override {
    DCHECK_NE(mode_, kReplay);
    if (mode_ == kDirect) {
      CommandBufferDirect::Flush(put_offset);
    } else {
      DCHECK_GE(put_offset, saved_put_offset_);
      saved_put_offset_ = put_offset;
    }
  }

  CommandBuffer::State WaitForTokenInRange(int32_t start,
                                           int32_t end) override {
    DCHECK_EQ(mode_, kDirect);
    return CommandBufferDirect::WaitForTokenInRange(start, end);
  }

  CommandBuffer::State WaitForGetOffsetInRange(uint32_t set_get_buffer_count,
                                               int32_t start,
                                               int32_t end) override {
    DCHECK_EQ(mode_, kDirect);
    return CommandBufferDirect::WaitForGetOffsetInRange(set_get_buffer_count,
                                                        start, end);
  }

  void SetGetBuffer(int32_t transfer_buffer_id) override {
    switch (mode_) {
      case kDirect:
        current_get_buffer_ = transfer_buffer_id;
        CommandBufferDirect::SetGetBuffer(transfer_buffer_id);
        break;
      case kRecord:
        DCHECK_EQ(saved_get_buffer_, -1);
        saved_get_buffer_ = transfer_buffer_id;
        break;
      case kReplay:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  void OnParseError() override {
    ASSERT_EQ(service()->GetState().error, error::kNoError);
  }

  void Replay() {
    DCHECK_EQ(mode_, kReplay);
    SetGetOffsetForTest(0);
    CommandBufferDirect::Flush(saved_put_offset_);
  }

  int32_t saved_put_offset() const { return saved_put_offset_; }
  Mode mode() const { return mode_; }

 private:
  Mode mode_ = kDirect;
  int32_t saved_put_offset_ = 0;
  int32_t saved_get_buffer_ = -1;
  int32_t current_get_buffer_ = -1;
};

GpuPreferences GetGpuPreferences() {
  GpuPreferences preferences;
  if (gles2::UsePassthroughCommandDecoder(
          base::CommandLine::ForCurrentProcess()))
    preferences.use_passthrough_cmd_decoder = true;
  return preferences;
}

// This wraps a RecordReplayCommandBuffer and gives it a back-end decoder, as
// well as a front-end GLES2Implementation. This allows recording commands at
// the GL level and replaying them to the driver (or a stub).
class RecordReplayContext : public GpuControl {
 public:
  RecordReplayContext()
      : gpu_preferences_(GetGpuPreferences()),
        share_group_(new gl::GLShareGroup),
        discardable_manager_(gpu::GpuPreferences()),
        passthrough_discardable_manager_(gpu::GpuPreferences()),
        translator_cache_(gpu_preferences_) {
    bool bind_generates_resource = false;
    if (base::CommandLine::ForCurrentProcess()->HasSwitch("use-stub")) {
      surface_ = new gl::GLSurfaceStub;
      scoped_refptr<gl::GLContextStub> context_stub =
          new gl::GLContextStub(share_group_.get());
      context_stub->SetGLVersionString("OpenGL ES 3.1");
      context_stub->SetUseStubApi(true);
      context_ = context_stub;
    } else {
      gl::GLContextAttribs attribs;
      if (gpu_preferences_.use_passthrough_cmd_decoder) {
        attribs.bind_generates_resource = bind_generates_resource;
        attribs.allow_client_arrays = false;
      }
      surface_ = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(),
                                                    gfx::Size());
      context_ = gl::init::CreateGLContext(share_group_.get(), surface_.get(),
                                           attribs);
    }
    context_->MakeCurrent(surface_.get());

    scoped_refptr<gles2::FeatureInfo> feature_info = new gles2::FeatureInfo();
    scoped_refptr<gles2::ContextGroup> context_group = new gles2::ContextGroup(
        gpu_preferences_, true, nullptr /* memory_tracker */,
        &translator_cache_, &completeness_cache_, feature_info,
        bind_generates_resource, nullptr /* progress_reporter */,
        GpuFeatureInfo(), &discardable_manager_,
        &passthrough_discardable_manager_, &shared_image_manager_);
    command_buffer_ = std::make_unique<RecordReplayCommandBuffer>();

    decoder_.reset(gles2::GLES2Decoder::Create(
        command_buffer_.get(), command_buffer_->service(), &outputter_,
        context_group.get()));
    command_buffer_->set_handler(decoder_.get());

    decoder_->GetLogger()->set_log_synthesized_gl_errors(false);

    ContextCreationAttribs attrib_helper;
    attrib_helper.context_type = CONTEXT_TYPE_OPENGLES3;

    ContextResult result =
        decoder_->Initialize(surface_.get(), context_.get(), true,
                             gles2::DisallowedFeatures(), attrib_helper);
    DCHECK_EQ(result, ContextResult::kSuccess);
    capabilities_ = decoder_->GetCapabilities();
    gl_capabilities_ = decoder_->GetGLCapabilities();

    const SharedMemoryLimits limits;
    gles2_helper_ =
        std::make_unique<gles2::GLES2CmdHelper>(command_buffer_.get());
    result = gles2_helper_->Initialize(limits.command_buffer_size);
    DCHECK_EQ(result, ContextResult::kSuccess);

    // Create a transfer buffer.
    transfer_buffer_ = std::make_unique<TransferBuffer>(gles2_helper_.get());

    // Create the object exposing the OpenGL API.
    const bool lose_context_when_out_of_memory = false;
    const bool support_client_side_arrays = false;
    gles2_implementation_ = std::make_unique<gles2::GLES2Implementation>(
        gles2_helper_.get(), nullptr, transfer_buffer_.get(),
        bind_generates_resource, lose_context_when_out_of_memory,
        support_client_side_arrays, this);

    result = gles2_implementation_->Initialize(limits);
    DCHECK_EQ(result, ContextResult::kSuccess);
  }

  ~RecordReplayContext() override {
    while (command_buffer_->mode() != RecordReplayCommandBuffer::kDirect)
      command_buffer_->AdvanceMode();
    gles2_implementation_.reset();
    transfer_buffer_.reset();
    gles2_helper_.reset();
    decoder_->Destroy(true);
    decoder_.reset();
    command_buffer_.reset();
  }

  void StartRecord() {
    DCHECK_EQ(command_buffer_->mode(), RecordReplayCommandBuffer::kDirect);
    gles2_helper_->FreeRingBuffer();
    command_buffer_->AdvanceMode();
  }

  void StartReplay() {
    DCHECK_EQ(command_buffer_->mode(), RecordReplayCommandBuffer::kRecord);
    gles2_helper_->FlushLazy();
    command_buffer_->AdvanceMode();
  }

  void Replay() { command_buffer_->Replay(); }

  gles2::GLES2Implementation* gl() { return gles2_implementation_.get(); }

 private:
  // GpuControl implementation;
  void SetGpuControlClient(GpuControlClient*) override {}

  const Capabilities& GetCapabilities() const override { return capabilities_; }

  const GLCapabilities& GetGLCapabilities() const override {
    return gl_capabilities_;
  }

  void SignalQuery(uint32_t query, base::OnceClosure callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void CancelAllQueries() override { NOTREACHED_IN_MIGRATION(); }

  void CreateGpuFence(uint32_t gpu_fence_id, ClientGpuFence source) override {
    NOTREACHED_IN_MIGRATION();
  }

  void GetGpuFence(uint32_t gpu_fence_id,
                   base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>
                       callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void SetLock(base::Lock*) override { NOTREACHED_IN_MIGRATION(); }

  void EnsureWorkVisible() override { NOTREACHED_IN_MIGRATION(); }

  gpu::CommandBufferNamespace GetNamespaceID() const override {
    return gpu::CommandBufferNamespace::INVALID;
  }

  CommandBufferId GetCommandBufferID() const override {
    return gpu::CommandBufferId();
  }

  void FlushPendingWork() override { NOTREACHED_IN_MIGRATION(); }

  uint64_t GenerateFenceSyncRelease() override {
    NOTREACHED_IN_MIGRATION();
    return 0;
  }

  bool IsFenceSyncReleased(uint64_t release) override {
    NOTREACHED_IN_MIGRATION();
    return true;
  }

  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       base::OnceClosure callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void WaitSyncToken(const gpu::SyncToken& sync_token) override {
    NOTREACHED_IN_MIGRATION();
  }

  bool CanWaitUnverifiedSyncToken(const gpu::SyncToken& sync_token) override {
    NOTREACHED_IN_MIGRATION();
    return true;
  }

  GpuPreferences gpu_preferences_;

  scoped_refptr<gl::GLShareGroup> share_group_;
  ServiceDiscardableManager discardable_manager_;
  PassthroughDiscardableManager passthrough_discardable_manager_;
  SharedImageManager shared_image_manager_;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;

  gles2::ShaderTranslatorCache translator_cache_;
  gles2::FramebufferCompletenessCache completeness_cache_;

  std::unique_ptr<RecordReplayCommandBuffer> command_buffer_;

  gles2::TraceOutputter outputter_;
  std::unique_ptr<gles2::GLES2Decoder> decoder_;
  gpu::Capabilities capabilities_;
  gpu::GLCapabilities gl_capabilities_;

  std::unique_ptr<gles2::GLES2CmdHelper> gles2_helper_;
  std::unique_ptr<TransferBuffer> transfer_buffer_;
  std::unique_ptr<gles2::GLES2Implementation> gles2_implementation_;
};

// This abstracts the performance capture loop, iterating through a warmup run
// and then a number of performance capturing runs.
class PerfIterator {
 public:
  PerfIterator(std::string story, int runs, int iterations)
      : story_(std::move(story)), runs_(runs), iterations_(iterations) {
    // When running under linux-perf, we try to isolate the microbenchmark
    // performance:
    // 1- sleep 1 second after warmup so that one can skip perf for
    // intitialization with 'perf record -D 1000'
    // 2- exit immediately after the capture loop is finished to skip teardown.
    // 3- avoid unneeded syscalls (time, print).
    for_linux_perf_ =
        base::CommandLine::ForCurrentProcess()->HasSwitch("for-linux-perf");
    if (base::CommandLine::ForCurrentProcess()->HasSwitch("fast-run")) {
      runs_ = 1;
      iterations_ = 100;
    }
  }

  PerfIterator(const PerfIterator&) = delete;
  PerfIterator& operator=(const PerfIterator&) = delete;

  bool Iterate() {
    if (--current_iterations_ > 0)
      return true;
    return NextOuter();
  }

 private:
  bool NextOuter() {
    base::TimeTicks time;
    if (warmup_) {
      warmup_ = false;
      if (for_linux_perf_)
        base::PlatformThread::Sleep(base::Seconds(1));
      else
        time = base::TimeTicks::Now();
    } else if (!for_linux_perf_) {
      time = base::TimeTicks::Now();
      double ns = (time - run_start_time_).InNanoseconds() / iterations_;
      perf_test::PerfResultReporter reporter("Decoder.", story_);
      reporter.RegisterImportantMetric("draw_wall_time", "ns");
      reporter.AddResult("draw_wall_time", ns);
    }
    if (runs_ == 0) {
      if (for_linux_perf_)
        base::Process::TerminateCurrentProcessImmediately(0);
      return false;
    }
    --runs_;
    current_iterations_ = iterations_;
    run_start_time_ = time;
    return true;
  }

  static constexpr int kWarmupIterations = 2;

  std::string story_;
  base::TimeTicks run_start_time_;
  int runs_;
  int iterations_;
  int current_iterations_ = 1 + kWarmupIterations;
  bool warmup_ = true;
  bool for_linux_perf_ = false;
};

class DecoderPerfTest : public testing::Test {
 public:
  ~DecoderPerfTest() override = default;

  void SetUp() override {
    context_ = std::make_unique<RecordReplayContext>();
    gl_ = context_->gl();
    gl_->GenRenderbuffers(1, &renderbuffer_);
    gl_->BindRenderbuffer(GL_RENDERBUFFER, renderbuffer_);
    gl_->RenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 256, 256);
    gl_->GenFramebuffers(1, &framebuffer_);
    gl_->BindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    gl_->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                 GL_RENDERBUFFER, renderbuffer_);
    gl_->Viewport(0, 0, 256, 256);
  }

  void StartRecord() { context_->StartRecord(); }

  void StartReplay() { context_->StartReplay(); }

  void Replay() { context_->Replay(); }

  GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = gl_->CreateShader(type);
    GLint length = base::checked_cast<GLint>(strlen(source));
    gl_->ShaderSource(shader, 1, &source, &length);
    gl_->CompileShader(shader);
    GLint compile_status = 0;
    gl_->GetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
    if (!compile_status) {
      GLint log_length = 0;
      gl_->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
      if (log_length) {
        auto log = base::HeapArray<GLchar>::WithSize(log_length);
        GLsizei returned_log_length = 0;
        gl_->GetShaderInfoLog(shader, log_length, &returned_log_length,
                              log.data());
        LOG(ERROR) << std::string(log.data(), returned_log_length);
      }
      gl_->DeleteShader(shader);
      return 0;
    }
    return shader;
  }

  struct AttribBinding {
    const char* name;
    GLuint location;
  };

  GLuint CreateAndLinkProgram(
      const char* vertex_shader,
      const char* fragment_shader,
      const std::initializer_list<AttribBinding>& attrib_bindings) {
    GLuint program = gl_->CreateProgram();
    GLuint vshader = CompileShader(GL_VERTEX_SHADER, vertex_shader);
    DCHECK_NE(0u, vshader);
    gl_->AttachShader(program, vshader);
    gl_->DeleteShader(vshader);
    GLuint fshader = CompileShader(GL_FRAGMENT_SHADER, fragment_shader);
    DCHECK_NE(0u, fshader);
    gl_->AttachShader(program, fshader);
    gl_->DeleteShader(fshader);
    for (const auto& attrib : attrib_bindings)
      gl_->BindAttribLocation(program, attrib.location, attrib.name);
    gl_->LinkProgram(program);
    GLint link_status = 0;
    gl_->GetProgramiv(program, GL_LINK_STATUS, &link_status);
    DCHECK_EQ(link_status, GL_TRUE);
    return program;
  }

  void CreateBasicTexture(GLuint texture) {
    gl_->BindTexture(GL_TEXTURE_2D, texture);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, nullptr);
  }

 protected:
  std::unique_ptr<RecordReplayContext> context_;
  raw_ptr<gles2::GLES2Implementation> gl_;
  GLuint renderbuffer_ = 0;
  GLuint framebuffer_ = 0;
};

constexpr const char kVertexShader[] =
    "attribute vec2 position;\n"
    "uniform vec2 scale;\n"
    "uniform vec2 offset;\n"
    "varying vec2 texcoords;\n"
    "void main () {\n"
    "  gl_Position = vec4(position * scale + offset, 0.0, 1.0);\n"
    "  texcoords = position;\n"
    "}\n";

constexpr const char kFragmentShader[] =
    "precision mediump float;\n"
    "varying vec2 texcoords;\n"
    "uniform sampler2D texture;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(texture, texcoords);\n"
    "}\n";

constexpr const float kVertices[] = {
    0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f,
};

// Measures a loop with Uniform2f and DrawArrays.
TEST_F(DecoderPerfTest, BasicDraw) {
  GLuint program =
      CreateAndLinkProgram(kVertexShader, kFragmentShader, {{"postition", 0}});
  gl_->UseProgram(program);

  GLint scale_location = gl_->GetUniformLocation(program, "scale");
  GLint offset_location = gl_->GetUniformLocation(program, "offset");
  GLint texture_location = gl_->GetUniformLocation(program, "texture");

  GLuint texture;
  gl_->GenTextures(1, &texture);
  CreateBasicTexture(texture);

  GLuint buffer;
  gl_->GenBuffers(1, &buffer);
  gl_->BindBuffer(GL_ARRAY_BUFFER, buffer);
  gl_->BufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices,
                  GL_STATIC_DRAW);

  gl_->VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat),
                           nullptr);
  gl_->EnableVertexAttribArray(0);

  gl_->Uniform1i(texture_location, 0);

  constexpr int N = 10;
  gl_->Uniform2f(scale_location, 2.f / N, 2.f / N);

  StartRecord();
  for (int x = 0; x < N; ++x) {
    float xpos = 2.f * x / N - 1.f;
    for (int y = 0; y < N; ++y) {
      float ypos = 2.f * y / N - 1.f;
      gl_->Uniform2f(offset_location, xpos, ypos);
      gl_->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
  }

  StartReplay();
  PerfIterator iterator("basic_draw_100", kDefaultRuns, kDefaultIterations);
  while (iterator.Iterate())
    Replay();
}

// Measures a loop with changing the texture binding between draws.
TEST_F(DecoderPerfTest, TextureDraw) {
  GLuint program =
      CreateAndLinkProgram(kVertexShader, kFragmentShader, {{"position", 0}});
  gl_->UseProgram(program);

  GLint scale_location = gl_->GetUniformLocation(program, "scale");
  GLint offset_location = gl_->GetUniformLocation(program, "offset");
  GLint texture_location = gl_->GetUniformLocation(program, "texture");

  constexpr size_t kTextures = 16;
  GLuint textures[kTextures];
  gl_->GenTextures(kTextures, textures);
  for (GLuint texture : textures)
    CreateBasicTexture(texture);

  GLuint buffer;
  gl_->GenBuffers(1, &buffer);
  gl_->BindBuffer(GL_ARRAY_BUFFER, buffer);
  gl_->BufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices,
                  GL_STATIC_DRAW);

  gl_->VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat),
                           nullptr);
  gl_->EnableVertexAttribArray(0);

  gl_->Uniform1i(texture_location, 0);

  constexpr int N = 10;
  gl_->Uniform2f(scale_location, 2.f / N, 2.f / N);

  StartRecord();
  size_t texture = 0;
  for (int x = 0; x < N; ++x) {
    float xpos = 2.f * x / N - 1.f;
    for (int y = 0; y < N; ++y) {
      float ypos = 2.f * y / N - 1.f;
      gl_->BindTexture(GL_TEXTURE_2D, textures[texture]);
      gl_->Uniform2f(offset_location, xpos, ypos);
      gl_->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      texture = (texture + 1) % kTextures;
    }
  }

  StartReplay();
  PerfIterator iterator("texture_draw_100", kDefaultRuns, kDefaultIterations);
  while (iterator.Iterate())
    Replay();
}

// Measures a loop with changing the program between draws.
TEST_F(DecoderPerfTest, ProgramDraw) {
  const char kVertexShader2[] =
      "attribute vec2 position;\n"
      "uniform vec2 scale;\n"
      "uniform vec2 offset;\n"
      "void main () {\n"
      "  gl_Position = vec4(position * scale + offset, 0.0, 1.0);\n"
      "}\n";

  const char kFragmentShader2[] =
      "precision mediump float;\n"
      "uniform vec4 color;\n"
      "void main() {\n"
      "  gl_FragColor = color;\n"
      "}\n";

  GLuint programs[2];
  programs[0] =
      CreateAndLinkProgram(kVertexShader, kFragmentShader, {{"position", 0}});

  GLint scale_location1 = gl_->GetUniformLocation(programs[0], "scale");
  GLint texture_location1 = gl_->GetUniformLocation(programs[0], "texture");

  programs[1] =
      CreateAndLinkProgram(kVertexShader2, kFragmentShader2, {{"position", 0}});

  GLint scale_location2 = gl_->GetUniformLocation(programs[1], "scale");
  GLint color_location2 = gl_->GetUniformLocation(programs[1], "color");

  GLuint texture;
  gl_->GenTextures(1, &texture);
  CreateBasicTexture(texture);

  GLuint buffer;
  gl_->GenBuffers(1, &buffer);
  gl_->BindBuffer(GL_ARRAY_BUFFER, buffer);
  gl_->BufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices,
                  GL_STATIC_DRAW);

  gl_->VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat),
                           nullptr);
  gl_->EnableVertexAttribArray(0);

  constexpr int N = 10;

  gl_->UseProgram(programs[0]);

  gl_->Uniform1i(texture_location1, 0);
  gl_->Uniform2f(scale_location1, 2.f / N, 2.f / N);

  gl_->UseProgram(programs[1]);
  gl_->Uniform2f(scale_location2, 2.f / N, 2.f / N);
  gl_->Uniform4f(color_location2, 1.f, 0.f, 0.f, 1.f);

  GLint offset_locations[2] = {gl_->GetUniformLocation(programs[0], "offset"),
                               gl_->GetUniformLocation(programs[1], "offset")};

  StartRecord();
  size_t program = 0;
  for (int x = 0; x < N; ++x) {
    float xpos = 2.f * x / N - 1.f;
    for (int y = 0; y < N; ++y) {
      float ypos = 2.f * y / N - 1.f;
      gl_->UseProgram(programs[program]);
      gl_->Uniform2f(offset_locations[program], xpos, ypos);
      gl_->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      program = 1 - program;
    }
  }

  StartReplay();
  PerfIterator iterator("program_draw_100", kDefaultRuns, kDefaultIterations);
  while (iterator.Iterate())
    Replay();
}

}  // anonymous namespace
}  // namespace gpu
