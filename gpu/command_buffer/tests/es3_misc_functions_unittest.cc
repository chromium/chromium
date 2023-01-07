// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_switches.h"

#define SHADER_VERSION_300(Src) "#version 300 es\n" #Src

namespace gpu {

class OpenGLES3FunctionTest : public testing::Test {
 protected:
  void SetUp() override {
    GLManager::Options options;
    options.context_type = CONTEXT_TYPE_OPENGLES3;
    gl_.Initialize(options);
  }
  void TearDown() override { gl_.Destroy(); }
  bool IsApplicable() const { return gl_.IsInitialized(); }
  GLManager gl_;
};

#if BUILDFLAG(IS_ANDROID)
// Test is failing for Lollipop 64 bit Tester.
// See crbug/550292.
#define MAYBE_GetFragDataLocationInvalid DISABLED_GetFragDataLocationInvalid
#else
#define MAYBE_GetFragDataLocationInvalid GetFragDataLocationInvalid
#endif
TEST_F(OpenGLES3FunctionTest, MAYBE_GetFragDataLocationInvalid) {
  if (!IsApplicable()) {
    return;
  }
  // clang-format off
  static const char* kVertexShader =
      SHADER_VERSION_300(
          in vec4 position;
          void main() {
            gl_Position = position;
          });
  static const char* kFragColorShader =
      SHADER_VERSION_300(
          precision mediump float;
          uniform vec4 src;
          out vec4 FragColor;
          void main() {
            FragColor = src;
          });
  // clang-format on

  GLuint vsid = GLTestHelper::LoadShader(GL_VERTEX_SHADER, kVertexShader);
  GLuint fsid = GLTestHelper::LoadShader(GL_FRAGMENT_SHADER, kFragColorShader);
  GLuint program = glCreateProgram();
  glAttachShader(program, vsid);
  glAttachShader(program, fsid);
  glDeleteShader(vsid);
  glDeleteShader(fsid);

  GLint location = glGetFragDataLocation(program, "FragColor");
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_EQ(-1, location);
  location = glGetFragDataLocation(program, "Unknown");
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_EQ(-1, location);

  glLinkProgram(program);

  location = glGetFragDataLocation(program, "FragColor");
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  EXPECT_EQ(0, location);
  location = glGetFragDataLocation(program, "Unknown");
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  EXPECT_EQ(-1, location);

  glDeleteProgram(program);
}

TEST_F(OpenGLES3FunctionTest, GetStringiTest) {
  if (!IsApplicable()) {
    return;
  }
  std::string extensionString =
      reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
  std::vector<std::string> extensions =
      base::SplitString(extensionString, base::kWhitespaceASCII,
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int num_extensions = 0;
  glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
  EXPECT_EQ(extensions.size(), static_cast<size_t>(num_extensions));
  std::set<std::string> extensions_from_string(extensions.begin(),
                                               extensions.end());
  std::set<std::string> extensions_from_stringi;
  for (int i = 0; i < num_extensions; ++i) {
    extensions_from_stringi.insert(
        reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i)));
  }
  EXPECT_EQ(extensions_from_string, extensions_from_stringi);
}

}  // namespace gpu
