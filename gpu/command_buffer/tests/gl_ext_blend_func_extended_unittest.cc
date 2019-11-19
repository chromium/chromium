// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include "base/numerics/ranges.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#define SHADER(Src) #Src
#define BFE_SHADER(Src) "#extension GL_EXT_blend_func_extended : require\n" #Src

namespace {
// Partial implementation of weight function for GLES 2 blend equation that
// is dual-source aware.
template <int factor, int index>
float Weight(float /*dst*/[4], float src[4], float src1[4]) {
  if (factor == GL_SRC_COLOR)
    return src[index];
  if (factor == GL_SRC_ALPHA)
    return src[3];
  if (factor == GL_SRC1_COLOR_EXT)
    return src1[index];
  if (factor == GL_SRC1_ALPHA_EXT)
    return src1[3];
  if (factor == GL_ONE_MINUS_SRC1_COLOR_EXT)
    return 1.0f - src1[index];
  if (factor == GL_ONE_MINUS_SRC1_ALPHA_EXT)
    return 1.0f - src1[3];
  return 0.0f;
}

// Implementation of GLES 2 blend equation that is dual-source aware.
template <int RGBs, int RGBd, int As, int Ad>
void BlendEquationFuncAdd(float dst[4],
                          float src[4],
                          float src1[4],
                          uint8_t result[4]) {
  float r[4];
  r[0] = src[0] * Weight<RGBs, 0>(dst, src, src1) +
         dst[0] * Weight<RGBd, 0>(dst, src, src1);
  r[1] = src[1] * Weight<RGBs, 1>(dst, src, src1) +
         dst[1] * Weight<RGBd, 1>(dst, src, src1);
  r[2] = src[2] * Weight<RGBs, 2>(dst, src, src1) +
         dst[2] * Weight<RGBd, 2>(dst, src, src1);
  r[3] = src[3] * Weight<As, 3>(dst, src, src1) +
         dst[3] * Weight<Ad, 3>(dst, src, src1);
  for (int i = 0; i < 4; ++i) {
    result[i] = static_cast<uint8_t>(
        std::floor(base::ClampToRange(r[i], 0.0f, 1.0f) * 255.0f));
  }
}

}  // namespace

namespace gpu {

class EXTBlendFuncExtendedTest : public testing::Test {
 public:
 protected:
  void SetUp() override { gl_.Initialize(GLManager::Options()); }

  void TearDown() override { gl_.Destroy(); }
  bool IsApplicable() const {
    return GLTestHelper::HasExtension("GL_EXT_blend_func_extended");
  }
  GLManager gl_;
};

TEST_F(EXTBlendFuncExtendedTest, TestMaxDualSourceDrawBuffers) {
  if (!IsApplicable())
    return;

  GLint maxDualSourceDrawBuffers = 0;
  glGetIntegerv(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT, &maxDualSourceDrawBuffers);
  EXPECT_GT(maxDualSourceDrawBuffers, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

class EXTBlendFuncExtendedDrawTest : public testing::TestWithParam<bool> {
 public:
  static const GLsizei kWidth = 100;
  static const GLsizei kHeight = 100;
  EXTBlendFuncExtendedDrawTest() : program_(0) {}

 protected:
  void SetUp() override {
    GLManager::Options options;
    options.size = gfx::Size(kWidth, kHeight);
    options.force_shader_name_hashing = GetParam();
    gl_.Initialize(options);
  }

  bool IsApplicable() const {
    return GLTestHelper::HasExtension("GL_EXT_blend_func_extended");
  }

  virtual const char* GetVertexShader() {
    // clang-format off
    static const char* kVertexShader =
        SHADER(
            attribute vec4 position;
            void main() {
              gl_Position = position;
            });
    // clang-format on
    return kVertexShader;
  }

  void CreateProgramWithFragmentShader(const char* fragment_shader_str) {
    GLuint vertex_shader =
        GLTestHelper::LoadShader(GL_VERTEX_SHADER, GetVertexShader());
    GLuint fragment_shader =
        GLTestHelper::LoadShader(GL_FRAGMENT_SHADER, fragment_shader_str);
    ASSERT_NE(0u, vertex_shader);
    ASSERT_NE(0u, fragment_shader);
    program_ = glCreateProgram();
    ASSERT_NE(0u, program_);
    glAttachShader(program_, vertex_shader);
    glAttachShader(program_, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
  }

  testing::AssertionResult LinkProgram() {
    glLinkProgram(program_);
    GLint linked = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (linked == 0) {
      char buffer[1024];
      GLsizei length = 0;
      glGetProgramInfoLog(program_, sizeof(buffer), &length, buffer);
      std::string log(buffer, length);
      return testing::AssertionFailure() << "Error linking program: " << log;
    }
    glUseProgram(program_);
    position_loc_ = glGetAttribLocation(program_, "position");
    src_loc_ = glGetUniformLocation(program_, "src");
    src1_loc_ = glGetUniformLocation(program_, "src1");
    return testing::AssertionSuccess();
  }

  void TearDown() override {
    if (program_ != 0)
      glDeleteProgram(program_);
    gl_.Destroy();
  }

  void DrawAndVerify() {
    float kDst[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float kSrc[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float kSrc1[4] = {0.3f, 0.6f, 0.9f, 0.7f};

    glUniform4f(src_loc_, kSrc[0], kSrc[1], kSrc[2], kSrc[3]);
    glUniform4f(src1_loc_, kSrc1[0], kSrc1[1], kSrc1[2], kSrc1[3]);

    GLTestHelper::SetupUnitQuad(position_loc_);

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC1_COLOR_EXT, GL_SRC_ALPHA,
                        GL_ONE_MINUS_SRC1_COLOR_EXT,
                        GL_ONE_MINUS_SRC1_ALPHA_EXT);
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

    // Draw one triangle (bottom left half).
    glViewport(0, 0, kWidth, kHeight);
    glClearColor(kDst[0], kDst[1], kDst[2], kDst[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    // Verify.
    uint8_t color[4];
    BlendEquationFuncAdd<GL_SRC1_COLOR_EXT, GL_SRC_ALPHA,
                         GL_ONE_MINUS_SRC1_COLOR_EXT,
                         GL_ONE_MINUS_SRC1_ALPHA_EXT>(kDst, kSrc, kSrc1, color);

    EXPECT_TRUE(GLTestHelper::CheckPixels(kWidth / 4, (3 * kHeight) / 4, 1, 1,
                                          1, color, nullptr));
    EXPECT_TRUE(
        GLTestHelper::CheckPixels(kWidth - 1, 0, 1, 1, 1, color, nullptr));
  }

 protected:
  GLuint program_;
  GLuint position_loc_;
  GLuint src_loc_;
  GLuint src1_loc_;
  GLManager gl_;
};

TEST_P(EXTBlendFuncExtendedDrawTest, ESSL1FragColor) {
  if (!IsApplicable())
    return;

  // Fails on AMDGPU-PRO driver crbug.com/786219
  gpu::GPUTestBotConfig bot_config;
  if (bot_config.LoadCurrentConfig(nullptr) &&
      bot_config.Matches("linux amd")) {
    return;
  }

  // clang-format off
  static const char* kFragColorShader =
      BFE_SHADER(
          precision mediump float;
          uniform vec4 src;
          uniform vec4 src1;
          void main() {
            gl_FragColor = src;
            gl_SecondaryFragColorEXT = src1;
          });
  // clang-format on
  CreateProgramWithFragmentShader(kFragColorShader);
  LinkProgram();
  DrawAndVerify();
}

TEST_P(EXTBlendFuncExtendedDrawTest, ESSL1FragData) {
  if (!IsApplicable())
    return;

  // Fails on the Intel Mesa driver, see
  // https://bugs.freedesktop.org/show_bug.cgi?id=96617
  gpu::GPUTestBotConfig bot_config;
  if (bot_config.LoadCurrentConfig(nullptr) &&
      bot_config.Matches("linux intel")) {
    return;
  }

  // clang-format off
  static const char* kFragDataShader =
      BFE_SHADER(
          precision mediump float;
          uniform vec4 src;
          uniform vec4 src1;
          void main() {
            gl_FragData[0] = src;
            gl_SecondaryFragDataEXT[0] = src1;
          });
  // clang-format on
  CreateProgramWithFragmentShader(kFragDataShader);
  LinkProgram();
  DrawAndVerify();
}

class EXTBlendFuncExtendedES3DrawTest : public EXTBlendFuncExtendedDrawTest {
 protected:
  void SetUp() override {
    GLManager::Options options;
    options.size = gfx::Size(kWidth, kHeight);
    options.context_type = CONTEXT_TYPE_OPENGLES3;
    options.force_shader_name_hashing = GetParam();
    gl_.Initialize(options);
  }
  bool IsApplicable() const {
    return gl_.IsInitialized() && EXTBlendFuncExtendedDrawTest::IsApplicable();
  }
  const char* GetVertexShader() override {
    // clang-format off
    static const char* kVertexShader =
        "#version 300 es\n"
        SHADER(
            in vec4 position;
            void main() {
              gl_Position = position;
            });
    // clang-format on
    return kVertexShader;
  }
};

TEST_P(EXTBlendFuncExtendedES3DrawTest, ESSL3Var) {
  if (!IsApplicable())
    return;
  // clang-format off
  static const char* kFragColorShader =
      "#version 300 es\n"
      BFE_SHADER(
          precision mediump float;
          uniform vec4 src;
          uniform vec4 src1;
          out vec4 FragColor;
          out vec4 SecondaryFragColor;
          void main() {
            FragColor = src;
            SecondaryFragColor = src1;
          });
  // clang-format on
  CreateProgramWithFragmentShader(kFragColorShader);
  glBindFragDataLocationIndexedEXT(program_, 0, 1, "SecondaryFragColor");
  LinkProgram();
  DrawAndVerify();
}

TEST_P(EXTBlendFuncExtendedES3DrawTest, ESSL3BindArrayWithSimpleName) {
  if (!IsApplicable())
    return;

  // Fails on the Intel Mesa driver, see
  // https://bugs.freedesktop.org/show_bug.cgi?id=96765
  gpu::GPUTestBotConfig bot_config;
  if (bot_config.LoadCurrentConfig(nullptr) &&
      bot_config.Matches("linux intel")) {
    return;
  }

  // clang-format off
  static const char* kFragDataShader =
      "#version 300 es\n"
      BFE_SHADER(
          precision mediump float;
          uniform vec4 src;
          uniform vec4 src1;
          out vec4 FragData[1];
          out vec4 SecondaryFragData[1];
          void main() {
            FragData[0] = src;
            SecondaryFragData[0] = src1;
          });
  // clang-format on
  CreateProgramWithFragmentShader(kFragDataShader);
  glBindFragDataLocationEXT(program_, 0, "FragData");
  glBindFragDataLocationIndexedEXT(program_, 0, 1, "SecondaryFragData");
  LinkProgram();
  DrawAndVerify();
}

TEST_P(EXTBlendFuncExtendedES3DrawTest, ESSL3BindSimpleVarAsArrayNoBind) {
  if (!IsApplicable())
    return;
  // clang-format off
  static const char* kFragDataShader =
      "#version 300 es\n"
      BFE_SHADER(
          precision mediump float;
          uniform vec4 src;
          uniform vec4 src1;
          out vec4 FragData;
          out vec4 SecondaryFragData;
          void main() {
            FragData = src;
            SecondaryFragData = src1;
          });
  // clang-format on
  CreateProgramWithFragmentShader(kFragDataShader);
  glBindFragDataLocationEXT(program_, 0, "FragData[0]");
  glBindFragDataLocationIndexedEXT(program_, 0, 1, "SecondaryFragData[0]");
  // Does not fail, since FragData[0] and SecondaryFragData[0] do not exist.
  EXPECT_TRUE(LinkProgram());

  EXPECT_EQ(-1, glGetFragDataLocation(program_, "FragData[0]"));
  EXPECT_EQ(0, glGetFragDataLocation(program_, "FragData"));
  EXPECT_EQ(1, glGetFragDataLocation(program_, "SecondaryFragData"));
  // Did not bind index.
  EXPECT_EQ(0, glGetFragDataIndexEXT(program_, "SecondaryFragData"));

  glBindFragDataLocationEXT(program_, 0, "FragData");
  glBindFragDataLocationIndexedEXT(program_, 0, 1, "SecondaryFragData");
  EXPECT_TRUE(LinkProgram());
  DrawAndVerify();
}

TEST_P(EXTBlendFuncExtendedES3DrawTest, ESSL3BindArrayAsArray) {
  if (!IsApplicable())
    return;

  // Fails on the Intel Mesa driver, see
  // https://bugs.freedesktop.org/show_bug.cgi?id=96765
  gpu::GPUTestBotConfig bot_config;
  if (bot_config.LoadCurrentConfig(nullptr) &&
      bot_config.Matches("linux intel")) {
    return;
  }

  // clang-format off
  static const char* kFragDataShader =
      "#version 300 es\n"
      BFE_SHADER(
          precision mediump float;
          uniform vec4 src;
          uniform vec4 src1;
          out vec4 FragData[1];
          out vec4 SecondaryFragData[1];
          void main() {
            FragData[0] = src;
            SecondaryFragData[0] = src1;
          });
  // clang-format on
  CreateProgramWithFragmentShader(kFragDataShader);
  glBindFragDataLocationEXT(program_, 0, "FragData[0]");
  glBindFragDataLocationIndexedEXT(program_, 0, 1, "SecondaryFragData[0]");
  LinkProgram();
  DrawAndVerify();
}

TEST_P(EXTBlendFuncExtendedES3DrawTest, ES3Getters) {
  if (!IsApplicable())
    return;
  // clang-format off
  static const char* kFragColorShader =
      "#version 300 es\n"
      BFE_SHADER(
          precision mediump float;
          uniform vec4 src;
          uniform vec4 src1;
          out vec4 FragColor;
          out vec4 SecondaryFragColor;
          void main() {
            FragColor = src;
            SecondaryFragColor = src1;
          });
  // clang-format on
  CreateProgramWithFragmentShader(kFragColorShader);
  glBindFragDataLocationEXT(program_, 0, "FragColor");
  glBindFragDataLocationIndexedEXT(program_, 0, 1, "SecondaryFragColor");

  // Getters return GL error before linking.
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  GLint location = glGetFragDataLocation(program_, "FragColor");
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  GLint index = glGetFragDataIndexEXT(program_, "FragColor");
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  location = glGetFragDataLocation(program_, "SecondaryFragColor");
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  index = glGetFragDataIndexEXT(program_, "SecondaryFragColor");
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  LinkProgram();

  // Getters return location and index after linking. Run twice to confirm that
  // setters do not affect the getters until next link.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE(testing::Message() << "Testing getters after link, iteration "
                                    << i);

    location = glGetFragDataLocation(program_, "FragColor");
    EXPECT_EQ(0, location);
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    index = glGetFragDataIndexEXT(program_, "FragColor");
    EXPECT_EQ(0, index);
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    location = glGetFragDataLocation(program_, "SecondaryFragColor");
    EXPECT_EQ(0, location);
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    index = glGetFragDataIndexEXT(program_, "SecondaryFragColor");
    EXPECT_EQ(1, index);
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

    // The calls should not affect the getters until re-linking.
    glBindFragDataLocationEXT(program_, 0, "SecondaryFragColor");
    glBindFragDataLocationIndexedEXT(program_, 0, 1, "FragColor");
  }

  LinkProgram();

  location = glGetFragDataLocation(program_, "FragColor");
  EXPECT_EQ(0, location);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  index = glGetFragDataIndexEXT(program_, "FragColor");
  EXPECT_EQ(1, index);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  location = glGetFragDataLocation(program_, "SecondaryFragColor");
  EXPECT_EQ(0, location);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  index = glGetFragDataIndexEXT(program_, "SecondaryFragColor");
  EXPECT_EQ(0, index);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Unknown colors return location -1, index -1.
  location = glGetFragDataLocation(program_, "UnknownColor");
  EXPECT_EQ(-1, location);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  index = glGetFragDataIndexEXT(program_, "UnknownColor");
  EXPECT_EQ(-1, index);

  // Reset the settings and verify that the driver gets them correct.
  glBindFragDataLocationEXT(program_, 0, "FragColor");
  glBindFragDataLocationIndexedEXT(program_, 0, 1, "SecondaryFragColor");
  LinkProgram();
  DrawAndVerify();
}

// Test that tests glBindFragDataLocationEXT, glBindFragDataLocationIndexedEXT,
// glGetFragDataLocation, glGetFragDataIndexEXT work correctly with
// GLSL array output variables. The output variable can be bound by
// referring to the variable name with or without the first element array
// accessor. The getters can query location of the individual elements in
// the array. The test does not actually use the base test drawing,
// since the drivers at the time of writing do not support multiple
// buffers and dual source blending.
TEST_P(EXTBlendFuncExtendedES3DrawTest, ES3GettersArray) {
  if (!IsApplicable())
    return;

  // TODO(zmo): Figure out why this fails on AMD. crbug.com/585132.
  // Also fails on the Intel Mesa driver, see
  // https://bugs.freedesktop.org/show_bug.cgi?id=96765
  gpu::GPUTestBotConfig bot_config;
  if (bot_config.LoadCurrentConfig(nullptr) &&
      (bot_config.Matches("linux amd") ||
      bot_config.Matches("linux intel"))) {
    return;
  }

  const GLint kTestArraySize = 2;
  const GLint kFragData0Location = 2;
  const GLint kFragData1Location = 1;
  const GLint kUnusedLocation = 5;

  // The test binds kTestArraySize -sized array to location 1 for test purposes.
  // The GL_MAX_DRAW_BUFFERS must be > kTestArraySize, since an
  // array will be bound to continuous locations, starting from the first
  // location.
  GLint maxDrawBuffers = 0;
  glGetIntegerv(GL_MAX_DRAW_BUFFERS_EXT, &maxDrawBuffers);
  EXPECT_LT(kTestArraySize, maxDrawBuffers);

  // clang-format off
  static const char* kFragColorShader =
      "#version 300 es\n"
      BFE_SHADER(
          precision mediump float;
          uniform vec4 src;
          uniform vec4 src1;
          out vec4 FragData[2];
          void main() {
            FragData[0] = src;
            FragData[1] = src1;
          });
  // clang-format on
  CreateProgramWithFragmentShader(kFragColorShader);

  for (int testcase = 0; testcase < 4; ++testcase) {
    if (testcase == 0) {
      glBindFragDataLocationEXT(program_, kUnusedLocation, "FragData[0]");
      glBindFragDataLocationEXT(program_, kFragData0Location, "FragData");
      glBindFragDataLocationEXT(program_, kFragData1Location, "FragData[1]");
    } else if (testcase == 1) {
      glBindFragDataLocationEXT(program_, kUnusedLocation, "FragData");
      glBindFragDataLocationEXT(program_, kFragData0Location, "FragData[0]");
      glBindFragDataLocationEXT(program_, kFragData1Location, "FragData[1]");
    } else if (testcase == 2) {
      glBindFragDataLocationIndexedEXT(program_, kUnusedLocation, 0,
                                       "FragData[0]");
      glBindFragDataLocationIndexedEXT(program_, kFragData0Location, 0,
                                       "FragData");
      glBindFragDataLocationIndexedEXT(program_, kFragData1Location, 0,
                                       "FragData[1]");
    } else if (testcase == 3) {
      glBindFragDataLocationIndexedEXT(program_, kUnusedLocation, 0,
                                       "FragData");
      glBindFragDataLocationIndexedEXT(program_, kFragData0Location, 0,
                                       "FragData[0]");
      glBindFragDataLocationIndexedEXT(program_, kFragData1Location, 0,
                                       "FragData[1]");
    }

    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
    LinkProgram();
    EXPECT_EQ(kFragData0Location, glGetFragDataLocation(program_, "FragData"));
    EXPECT_EQ(0, glGetFragDataIndexEXT(program_, "FragData"));
    EXPECT_EQ(kFragData0Location,
              glGetFragDataLocation(program_, "FragData[0]"));
    EXPECT_EQ(0, glGetFragDataIndexEXT(program_, "FragData[0]"));
    EXPECT_EQ(kFragData1Location,
              glGetFragDataLocation(program_, "FragData[1]"));
    EXPECT_EQ(0, glGetFragDataIndexEXT(program_, "FragData[1]"));

    // Index bigger than the GLSL variable array length does not find anything.
    EXPECT_EQ(-1, glGetFragDataLocation(program_, "FragData[3]"));
  }
}

// Test that tests glBindFragDataLocationEXT, glBindFragDataLocationIndexedEXT
// conflicts
// with GLSL output variables.
TEST_P(EXTBlendFuncExtendedES3DrawTest, ES3Conflicts) {
  if (!IsApplicable())
    return;
  const GLint kTestArraySize = 2;
  const GLint kColorName0Location = 0;
  const GLint kColorName1Location = 1;
  GLint maxDrawBuffers = 0;
  glGetIntegerv(GL_MAX_DRAW_BUFFERS_EXT, &maxDrawBuffers);
  EXPECT_LT(kTestArraySize, maxDrawBuffers);

  // clang-format off
  static const char* kFragColorShader =
      "#version 300 es\n"
      BFE_SHADER(
          precision mediump float;
          uniform vec4 src;
          uniform vec4 src1;
          out vec4 FragData0;
          out vec4 FragData1;
          void main() {
            FragData0 = src;
            FragData1 = src1;
          });
  // clang-format on
  CreateProgramWithFragmentShader(kFragColorShader);

  glBindFragDataLocationEXT(program_, kColorName0Location, "FragData0");
  glBindFragDataLocationEXT(program_, kColorName0Location, "FragData1");
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  EXPECT_FALSE(LinkProgram());

  glBindFragDataLocationIndexedEXT(program_, kColorName0Location, 0,
                                   "FragData0");
  glBindFragDataLocationIndexedEXT(program_, kColorName0Location, 0,
                                   "FragData1");
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  EXPECT_FALSE(LinkProgram());

  glBindFragDataLocationIndexedEXT(program_, kColorName0Location, 1,
                                   "FragData0");
  glBindFragDataLocationIndexedEXT(program_, kColorName0Location, 1,
                                   "FragData1");
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  EXPECT_FALSE(LinkProgram());

  // Test that correct binding actually works.
  glBindFragDataLocationEXT(program_, kColorName0Location, "FragData0");
  glBindFragDataLocationEXT(program_, kColorName1Location, "FragData1");
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  EXPECT_TRUE(LinkProgram());

  glBindFragDataLocationIndexedEXT(program_, kColorName0Location, 0,
                                   "FragData0");
  glBindFragDataLocationIndexedEXT(program_, kColorName0Location, 1,
                                   "FragData1");
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  EXPECT_TRUE(LinkProgram());
}

// Test that tests glBindFragDataLocationEXT conflicts
// with GLSL array output variables.
TEST_P(EXTBlendFuncExtendedES3DrawTest, ES3ConflictsArray) {
  if (!IsApplicable())
    return;
  const GLint kTestArraySize = 2;
  const GLint kColorName0Location = 0;
  const GLint kColorName1Location = 1;
  const GLint kUnusedLocation = 5;
  GLint maxDrawBuffers = 0;
  glGetIntegerv(GL_MAX_DRAW_BUFFERS_EXT, &maxDrawBuffers);
  EXPECT_LT(kTestArraySize, maxDrawBuffers);

  // clang-format off
  static const char* kFragColorShader =
      "#version 300 es\n"
      BFE_SHADER(
          precision mediump float;
          uniform vec4 src;
          uniform vec4 src1;
          out vec4 FragData[2];
          void main() {
            FragData[0] = src;
            FragData[1] = src1;
          });
  // clang-format on
  CreateProgramWithFragmentShader(kFragColorShader);

  glBindFragDataLocationEXT(program_, kColorName1Location, "FragData");
  glBindFragDataLocationEXT(program_, kColorName1Location, "FragData[1]");
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  EXPECT_FALSE(LinkProgram());
  glBindFragDataLocationEXT(program_, kUnusedLocation, "FragData");
  glBindFragDataLocationEXT(program_, kColorName1Location, "FragData[0]");
  glBindFragDataLocationEXT(program_, kColorName1Location, "FragData[1]");
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  EXPECT_FALSE(LinkProgram());
  // Test that binding actually works.
  glBindFragDataLocationEXT(program_, kColorName0Location, "FragData[0]");
  glBindFragDataLocationEXT(program_, kColorName1Location, "FragData[1]");
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  EXPECT_TRUE(LinkProgram());
}

INSTANTIATE_TEST_SUITE_P(TranslatorVariants,
                         EXTBlendFuncExtendedDrawTest,
                         ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(TranslatorVariants,
                         EXTBlendFuncExtendedES3DrawTest,
                         ::testing::Bool());

}  // namespace gpu
