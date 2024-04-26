// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stdint.h>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#define SHADER(Src) #Src

namespace gpu {

class BindUniformLocationTest : public testing::TestWithParam<bool> {
 protected:
  static const GLsizei kResolution = 4;
  void SetUp() override {
    GLManager::Options options;
    options.size = gfx::Size(kResolution, kResolution);
    options.force_shader_name_hashing = GetParam();
    gl_.Initialize(options);
  }

  void TearDown() override { gl_.Destroy(); }

  GLManager gl_;
};

TEST_P(BindUniformLocationTest, Basic) {
  ASSERT_TRUE(
      GLTestHelper::HasExtension("GL_CHROMIUM_bind_uniform_location"));

  static const char* v_shader_str = SHADER(
      attribute vec4 a_position;
      void main()
      {
         gl_Position = a_position;
      }
  );
  static const char* f_shader_str = SHADER(
      precision mediump float;
      uniform vec4 u_colorC;
      uniform vec4 u_colorB[2];
      uniform vec4 u_colorA;
      void main()
      {
        gl_FragColor = u_colorA + u_colorB[0] + u_colorB[1] + u_colorC;
      }
  );

  GLint color_a_location = 3;
  GLint color_b_location = 10;
  GLint color_c_location = 5;

  GLuint vertex_shader = GLTestHelper::LoadShader(
      GL_VERTEX_SHADER, v_shader_str);
  GLuint fragment_shader = GLTestHelper::LoadShader(
      GL_FRAGMENT_SHADER, f_shader_str);

  GLuint program = glCreateProgram();

  glBindUniformLocationCHROMIUM(program, color_a_location, "u_colorA");
  glBindUniformLocationCHROMIUM(program, color_b_location, "u_colorB[0]");
  glBindUniformLocationCHROMIUM(program, color_c_location, "u_colorC");

  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  // Link the program
  glLinkProgram(program);
  // Check the link status
  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  EXPECT_EQ(1, linked);

  GLint position_loc = glGetAttribLocation(program, "a_position");

  GLTestHelper::SetupUnitQuad(position_loc);

  glUseProgram(program);

  static const float color_b[] = {
    0.0f, 0.50f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.75f, 0.0f,
  };

  glUniform4f(color_a_location, 0.25f, 0.0f, 0.0f, 0.0f);
  glUniform4fv(color_b_location, 2, color_b);
  glUniform4f(color_c_location, 0.0f, 0.0f, 0.0f, 1.0f);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  static const uint8_t expected[] = {64, 128, 192, 255};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kResolution, kResolution, 1,
                                        expected, nullptr));

  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_P(BindUniformLocationTest, ConflictsDetection) {
  ASSERT_TRUE(
      GLTestHelper::HasExtension("GL_CHROMIUM_bind_uniform_location"));

  static const char* v_shader_str = SHADER(
      attribute vec4 a_position;
      void main()
      {
         gl_Position = a_position;
      }
  );
  static const char* f_shader_str = SHADER(
      precision mediump float;
      uniform vec4 u_colorA;
      uniform vec4 u_colorB;
      void main()
      {
        gl_FragColor = u_colorA + u_colorB;
      }
  );

  GLint color_a_location = 3;
  GLint color_b_location = 4;

  GLuint vertex_shader = GLTestHelper::LoadShader(
      GL_VERTEX_SHADER, v_shader_str);
  GLuint fragment_shader = GLTestHelper::LoadShader(
      GL_FRAGMENT_SHADER, f_shader_str);

  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);

  glBindUniformLocationCHROMIUM(program, color_a_location, "u_colorA");
  // Bind u_colorB to location a, causing conflicts, link should fail.
  glBindUniformLocationCHROMIUM(program, color_a_location, "u_colorB");
  glLinkProgram(program);
  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  EXPECT_EQ(0, linked);

  // Bind u_colorB to location b, no conflicts, link should succeed.
  glBindUniformLocationCHROMIUM(program, color_b_location, "u_colorB");
  glLinkProgram(program);
  linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  EXPECT_EQ(1, linked);

  GLTestHelper::CheckGLError("no errors", __LINE__);
}

// TODO(crbug.com/40246425): Flaky on Asan/Lsan builds.
#if defined(ADDRESS_SANITIZER) && defined(LEAK_SANITIZER)
#define MAYBE_Compositor DISABLED_Compositor
#else
#define MAYBE_Compositor Compositor
#endif
TEST_P(BindUniformLocationTest, MAYBE_Compositor) {
  ASSERT_TRUE(
      GLTestHelper::HasExtension("GL_CHROMIUM_bind_uniform_location"));

  static const char* v_shader_str = SHADER(
      attribute vec4 a_position;
      attribute vec2 a_texCoord;
      uniform mat4 matrix;
      uniform vec2 color_a[4];
      uniform vec4 color_b;
      varying vec4 v_color;
      void main()
      {
          v_color.xy = color_a[0] + color_a[1];
          v_color.zw = color_a[2] + color_a[3];
          v_color += color_b;
          gl_Position = matrix * a_position;
      }
  );

  static const char* f_shader_str =  SHADER(
      precision mediump float;
      varying vec4 v_color;
      uniform float alpha;
      uniform vec4 multiplier;
      uniform vec3 color_c[8];
      void main()
      {
          vec4 color_c_sum = vec4(0.0);
          color_c_sum.xyz += color_c[0];
          color_c_sum.xyz += color_c[1];
          color_c_sum.xyz += color_c[2];
          color_c_sum.xyz += color_c[3];
          color_c_sum.xyz += color_c[4];
          color_c_sum.xyz += color_c[5];
          color_c_sum.xyz += color_c[6];
          color_c_sum.xyz += color_c[7];
          color_c_sum.w = alpha;
          color_c_sum *= multiplier;
          gl_FragColor = v_color + color_c_sum;
      }
  );

  int counter = 6;
  int matrix_location = counter++;
  int color_a_location = counter++;
  int color_b_location = counter++;
  int alpha_location = counter++;
  int multiplier_location = counter++;
  int color_c_location = counter++;

  GLuint vertex_shader = GLTestHelper::LoadShader(
      GL_VERTEX_SHADER, v_shader_str);
  GLuint fragment_shader = GLTestHelper::LoadShader(
      GL_FRAGMENT_SHADER, f_shader_str);

  GLuint program = glCreateProgram();

  glBindUniformLocationCHROMIUM(program, matrix_location, "matrix");
  glBindUniformLocationCHROMIUM(program, color_a_location, "color_a");
  glBindUniformLocationCHROMIUM(program, color_b_location, "color_b");
  glBindUniformLocationCHROMIUM(program, alpha_location, "alpha");
  glBindUniformLocationCHROMIUM(program, multiplier_location, "multiplier");
  glBindUniformLocationCHROMIUM(program, color_c_location, "color_c");

  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  // Link the program
  glLinkProgram(program);
  // Check the link status
  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  EXPECT_EQ(1, linked);

  GLint position_loc = glGetAttribLocation(program, "a_position");

  GLTestHelper::SetupUnitQuad(position_loc);

  glUseProgram(program);

  static const float color_a[] = {
    0.1f, 0.1f, 0.1f, 0.1f,
    0.1f, 0.1f, 0.1f, 0.1f,
  };

  static const float color_c[] = {
    0.1f, 0.1f, 0.1f,
    0.1f, 0.1f, 0.1f,
    0.1f, 0.1f, 0.1f,
    0.1f, 0.1f, 0.1f,
    0.1f, 0.1f, 0.1f,
    0.1f, 0.1f, 0.1f,
    0.1f, 0.1f, 0.1f,
    0.1f, 0.1f, 0.1f,
  };

  static const float identity[] = {
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
  };

  glUniformMatrix4fv(matrix_location, 1, false, identity);
  glUniform2fv(color_a_location, 4, color_a);
  glUniform4f(color_b_location, 0.2f, 0.2f, 0.2f, 0.2f);
  glUniform1f(alpha_location, 0.8f);
  glUniform4f(multiplier_location, 0.5f, 0.5f, 0.5f, 0.5f);
  glUniform3fv(color_c_location, 8, color_c);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  static const uint8_t expected[] = {204, 204, 204, 204};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kResolution, kResolution, 1,
                                        expected, nullptr));

  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_P(BindUniformLocationTest, UnusedUniformUpdate) {
  ASSERT_TRUE(GLTestHelper::HasExtension("GL_CHROMIUM_bind_uniform_location"));

  // clang-format off
  static const char* kVertexShaderString = SHADER(
      attribute vec4 a_position;
      void main() {
        gl_Position = a_position;
      }
  );
  static const char* kFragmentShaderString = SHADER(
      precision mediump float;
      uniform vec4 u_colorA;
      uniform float u_colorU;
      uniform vec4 u_colorC;
      void main() {
        gl_FragColor = u_colorA + u_colorC;
      }
  );
  // clang-format on
  const GLint kColorULocation = 1;
  const GLint kNonexistingLocation = 5;
  const GLint kUnboundLocation = 6;

  GLuint vertex_shader =
      GLTestHelper::LoadShader(GL_VERTEX_SHADER, kVertexShaderString);
  GLuint fragment_shader =
      GLTestHelper::LoadShader(GL_FRAGMENT_SHADER, kFragmentShaderString);
  GLuint program = glCreateProgram();
  glBindUniformLocationCHROMIUM(program, kColorULocation, "u_colorU");
  // The non-existing uniform should behave like existing, but optimized away
  // uniform.
  glBindUniformLocationCHROMIUM(program, kNonexistingLocation, "nonexisting");
  // Let A and C be assigned automatic locations.
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);
  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  EXPECT_EQ(1, linked);
  glUseProgram(program);

  // No errors on bound locations, since caller does not know
  // if the driver optimizes them away or not.
  glUniform1f(kColorULocation, 0.25f);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // No errors on bound locations of names that do not exist
  // in the shader. Otherwise it would be inconsistent wrt the
  // optimization case.
  glUniform1f(kNonexistingLocation, 0.25f);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // The above are equal to updating -1.
  glUniform1f(-1, 0.25f);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // No errors when updating with other type either.
  // The type can not be known with the non-existing case.
  glUniform2f(kColorULocation, 0.25f, 0.25f);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glUniform2f(kNonexistingLocation, 0.25f, 0.25f);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glUniform2f(-1, 0.25f, 0.25f);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());

  // Ensure that driver or ANGLE has optimized the variable
  // away and the test tests what it is supposed to.
  EXPECT_EQ(-1, glGetUniformLocation(program, "u_colorU"));

  // The bound location gets marked as used and the driver
  // does not allocate other variables to that location.
  EXPECT_NE(kColorULocation, glGetUniformLocation(program, "u_colorA"));
  EXPECT_NE(kColorULocation, glGetUniformLocation(program, "u_colorC"));
  EXPECT_NE(kNonexistingLocation, glGetUniformLocation(program, "u_colorA"));
  EXPECT_NE(kNonexistingLocation, glGetUniformLocation(program, "u_colorC"));

  // Unintuitive: while specifying value works, getting the value does not.
  GLfloat get_result = 0.0f;
  glGetUniformfv(program, kColorULocation, &get_result);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  glGetUniformfv(program, kNonexistingLocation, &get_result);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  glGetUniformfv(program, -1, &get_result);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  // Updating an unbound, non-existing location still causes
  // an error.
  glUniform1f(kUnboundLocation, 0.25f);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
}

// Test for a bug where using a sampler caused GL error if the program had
// uniforms that were optimized away by the driver. This was only a problem with
// glBindUniformLocationCHROMIUM implementation. This could be reproed by
// binding the sampler to a location higher than the amount of active uniforms.
TEST_P(BindUniformLocationTest, UseSamplerWhenUnusedUniforms) {
  enum {
    kTexLocation = 54
  };
  // clang-format off
  static const char* vertexShaderString = SHADER(
      void main() {
        gl_Position = vec4(0);
      }
  );
  static const char* fragmentShaderString = SHADER(
      uniform sampler2D tex;
      void main() {
        gl_FragColor = texture2D(tex, vec2(1));
      }
  );
  // clang-format on
  GLuint vs = GLTestHelper::CompileShader(GL_VERTEX_SHADER, vertexShaderString);
  GLuint fs = GLTestHelper::CompileShader(GL_FRAGMENT_SHADER,
                                          fragmentShaderString);

  GLuint program = glCreateProgram();
  glBindUniformLocationCHROMIUM(program, kTexLocation, "tex");

  glAttachShader(program, vs);
  glAttachShader(program, fs);

  glLinkProgram(program);

  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  EXPECT_NE(0, linked);
  glUseProgram(program);
  glUniform1i(kTexLocation, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

INSTANTIATE_TEST_SUITE_P(WithAndWithoutShaderNameMapping,
                         BindUniformLocationTest,
                         ::testing::Bool());

}  // namespace gpu



