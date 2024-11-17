// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include <string_view>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_executor.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/command_buffer/tests/lpm/gl_lpm_fuzzer.pb.h"
#include "gpu/command_buffer/tests/lpm/gl_lpm_shader_to_string.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "ui/gfx/extension_set.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

// Enable this to log and crash on unexpected errors during shader compilation.
#define CHECK_FOR_UNKNOWN_ERRORS false

struct Env {
  Env() {
    CHECK(base::i18n::InitializeICU());
    base::CommandLine::Init(0, nullptr);
    auto* command_line = base::CommandLine::ForCurrentProcess();

    // TODO(nedwill): support switches for swiftshader, etc.
    command_line->AppendSwitchASCII(switches::kUseGL,
                                    gl::kGLImplementationANGLEName);
    command_line->AppendSwitchASCII(switches::kUseANGLE,
                                    gl::kANGLEImplementationNullName);
    base::FeatureList::InitInstance(std::string(), std::string());
    base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
#if BUILDFLAG(IS_OZONE)
    ui::OzonePlatform::InitializeForGPU(ui::OzonePlatform::InitParams());
#endif
    gpu::GLTestHelper::InitializeGLDefault();
    ::gles2::Initialize();
  }

  base::AtExitManager at_exit;
};

class ScopedGLManager {
 public:
  ScopedGLManager() {
    gpu::GLManager::Options options;
    options.context_type = gpu::CONTEXT_TYPE_OPENGLES3;
    gl_.Initialize(options);
  }
  ~ScopedGLManager() { gl_.Destroy(); }

 private:
  gpu::GLManager gl_;
};

GLuint CompileShader(GLenum type, const char* shaderSrc) {
  GLuint shader = glCreateShader(type);
  // Load the shader source
  glShaderSource(shader, 1, &shaderSrc, nullptr);
  // Compile the shader
  glCompileShader(shader);

  return shader;
}

// TODO(nedwill): Once the grammar stabilizes, try to remove as many
// of these as possible by making tweaks to the grammar/code generation.
const char* acceptable_errors[] = {
    "void function cannot return a value",
    "function already has a body",
    "undeclared identifier",
    "l-value required",
    "cannot convert from",
    "main function cannot return a value",
    "illegal use of type 'void'",
    "boolean expression expected",
    "Missing main()",
    "Divide by zero error during constant folding",
    "wrong operand types",
    "function must have the same return type in all of its declarations",
    "function return is not matching type",
    "redefinition",
    "WARNING:",
    "can't modify void",
    "No precision specified for",
    "exists that takes an operand of type",
    "Illegal use of reserved word",
    "'double' : syntax error",
    "Integer overflow",
    "dimension mismatch",
    "undeclared identifier",
    "comparison operator only defined for scalars",
    "Local variables can only use the const storage qualifier.",
    "must use 'flat' interpolation here",
    "invalid qualifier combination",
    "No precision specified for",
    "'out' : cannot be matrix",
    "non-void function must return a value",
    "function does not return a value",
    // Uniform, const, input can't be modified
    "can't modify a",
    "global variable initializers must be constant expressions",
    ("must explicitly specify all locations when using multiple fragment "
     "outputs"),
};

// Filter errors which we don't think interfere with fuzzing everything.
bool ErrorOk(std::string_view line) {
  for (const std::string_view acceptable_error : acceptable_errors) {
    if (base::Contains(line, acceptable_error)) {
      return true;
    }
  }
  LOG(WARNING) << "failed due to line: " << line;
  return false;
}

bool ErrorsOk(std::string_view log) {
  std::vector<std::string> lines = base::SplitString(
      log, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& line : lines) {
    if (!ErrorOk(line)) {
      return false;
    }
  }
  return true;
}

GLuint LoadShader(GLenum type, const fuzzing::Shader& shader_proto) {
  std::string shader_s = gl_lpm_fuzzer::GetShader(shader_proto);
  if (shader_s.empty()) {
    return 0;
  }

  GLuint shader = CompileShader(type, shader_s.c_str());

  // Check the compile status
  GLint value = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &value);
  if (value == 0) {
    if (CHECK_FOR_UNKNOWN_ERRORS) {
      GLint log_length = 0;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
      auto buffer = std::make_unique<GLchar[]>(log_length);
      glGetShaderInfoLog(shader, log_length, /*length=*/nullptr, buffer.get());
      std::string_view log(buffer.get(), log_length);
      if (!ErrorsOk(log)) {
        LOG(FATAL) << "Encountered an unexpected failure when translating:\n"
                   << log << "\nfailed to compile shader:\n"
                   << shader_proto.DebugString() << "converted:\n"
                   << shader_s;
      }
    }
    glDeleteShader(shader);
    shader = 0;
  }
  return shader;
}

// Same as GLTestHelper::SetupProgram but does not expect shaders
// to link successfully.
GLuint SetupProgram(GLuint vertex_shader, GLuint fragment_shader) {
  GLuint program =
      gpu::GLTestHelper::LinkProgram(vertex_shader, fragment_shader);
  // Check the link status
  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (linked == 0) {
    if (CHECK_FOR_UNKNOWN_ERRORS) {
      GLint log_length = 0;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
      auto buffer = std::make_unique<GLchar[]>(log_length);
      glGetProgramInfoLog(program, log_length, /*length=*/nullptr,
                          buffer.get());
      std::string_view log(buffer.get(), log_length);
      LOG(WARNING) << "Error linking program: " << log;
    }
    glDeleteProgram(program);
    program = 0;
  }
  return program;
}

DEFINE_PROTO_FUZZER(const fuzzing::Session& session) {
  static Env* env = new Env();
  CHECK(env);
  // TODO(nedwill): Creating a new GLManager on each iteration
  // is expensive. We should investigate ways to avoid expensive
  // initialization.
  ScopedGLManager scoped_gl_manager;

  GLuint vertex_shader_id =
      LoadShader(GL_VERTEX_SHADER, session.vertex_shader());
  GLuint fragment_shader_id =
      LoadShader(GL_FRAGMENT_SHADER, session.fragment_shader());
  if (!vertex_shader_id || !fragment_shader_id) {
    return;
  }

  GLuint program = SetupProgram(vertex_shader_id, fragment_shader_id);
  if (!program) {
    return;
  }

  glUseProgram(program);
  // Relink program.
  glLinkProgram(program);
}
