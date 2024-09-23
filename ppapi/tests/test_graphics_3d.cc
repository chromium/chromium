// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_graphics_3d.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/module.h"
#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"
#include "ppapi/tests/test_case.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

const int32_t kInvalidContext = 0;

REGISTER_TEST_CASE(Graphics3D);

bool TestGraphics3D::Init() {
  opengl_es2_ = static_cast<const PPB_OpenGLES2*>(
      pp::Module::Get()->GetBrowserInterface(PPB_OPENGLES2_INTERFACE));
  glInitializePPAPI(pp::Module::Get()->get_browser_interface());
  return opengl_es2_ && CheckTestingInterface();
}

void TestGraphics3D::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestGraphics3D, FramePPAPI, filter);
  RUN_CALLBACK_TEST(TestGraphics3D, FrameGL, filter);
  RUN_CALLBACK_TEST(TestGraphics3D, ExtensionsGL, filter);
  RUN_CALLBACK_TEST(TestGraphics3D, BadResource, filter);
  RUN_CALLBACK_TEST(TestGraphics3D, Attributes, filter);
}

// Tests that all valid context attributes are allowed.
std::string TestGraphics3D::TestAttributes() {
  const int width = 16;
  const int height = 16;
  const int32_t attribs[] = {PP_GRAPHICS3DATTRIB_ALPHA_SIZE,
                             8,
                             PP_GRAPHICS3DATTRIB_BLUE_SIZE,
                             8,
                             PP_GRAPHICS3DATTRIB_GREEN_SIZE,
                             8,
                             PP_GRAPHICS3DATTRIB_RED_SIZE,
                             8,
                             PP_GRAPHICS3DATTRIB_DEPTH_SIZE,
                             0,
                             PP_GRAPHICS3DATTRIB_STENCIL_SIZE,
                             0,
                             PP_GRAPHICS3DATTRIB_SAMPLES,
                             0,
                             PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS,
                             0,
                             PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR,
                             PP_GRAPHICS3DATTRIB_BUFFER_PRESERVED,
                             PP_GRAPHICS3DATTRIB_SINGLE_BUFFER,
                             0,
                             PP_GRAPHICS3DATTRIB_GPU_PREFERENCE,
                             PP_GRAPHICS3DATTRIB_GPU_PREFERENCE_LOW_POWER,
                             PP_GRAPHICS3DATTRIB_WIDTH,
                             width,
                             PP_GRAPHICS3DATTRIB_HEIGHT,
                             height,
                             PP_GRAPHICS3DATTRIB_NONE};
  pp::Graphics3D context(instance_, attribs);
  ASSERT_FALSE(context.is_null());
  PASS();
}

std::string TestGraphics3D::TestFramePPAPI() {
  const int width = 16;
  const int height = 16;
  const int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_WIDTH, width,
      PP_GRAPHICS3DATTRIB_HEIGHT, height,
      PP_GRAPHICS3DATTRIB_NONE
  };
  pp::Graphics3D context(instance_, attribs);
  ASSERT_FALSE(context.is_null());

  const uint8_t red_color[4] = {255, 0, 0, 255};

  // Access OpenGLES API through the PPAPI interface.
  // Clear color buffer to opaque red.
  opengl_es2_->ClearColor(context.pp_resource(), 1.0f, 0.0f, 0.0f, 1.0f);
  opengl_es2_->Clear(context.pp_resource(), GL_COLOR_BUFFER_BIT);
  // Check if the color buffer has opaque red.
  std::string error = CheckPixelPPAPI(&context, width/2, height/2, red_color);
  if (!error.empty())
    return error;

  int32_t rv = SwapBuffersSync(&context);
  ASSERT_EQ(PP_OK, rv);

  PASS();
}

std::string TestGraphics3D::TestFrameGL() {
  const int width = 16;
  const int height = 16;
  const int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_WIDTH, width,
      PP_GRAPHICS3DATTRIB_HEIGHT, height,
      PP_GRAPHICS3DATTRIB_NONE
  };
  pp::Graphics3D context(instance_, attribs);
  ASSERT_FALSE(context.is_null());

  const uint8_t red_color[4] = {255, 0, 0, 255};
  // Perform same operations as TestFramePPAPI, but use OpenGLES API directly.
  // This is how most developers will use OpenGLES.
  glSetCurrentContextPPAPI(context.pp_resource());
  glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  std::string error = CheckPixelGL(width/2, height/2, red_color);
  glSetCurrentContextPPAPI(kInvalidContext);
  if (!error.empty())
    return error;

  int32_t rv = SwapBuffersSync(&context);
  ASSERT_EQ(PP_OK, rv);

  PASS();
}

std::string TestGraphics3D::TestExtensionsGL() {
  const int width = 16;
  const int height = 16;
  const int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_WIDTH, width,
      PP_GRAPHICS3DATTRIB_HEIGHT, height,
      PP_GRAPHICS3DATTRIB_NONE
  };
  pp::Graphics3D context(instance_, attribs);
  ASSERT_FALSE(context.is_null());

  glSetCurrentContextPPAPI(context.pp_resource());
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Ask about a couple of extensions via glGetString.  If an extension is
  // available, try a couple of trivial calls.  This test is not intended
  // to be exhaustive; check the source can compile, link, and run without
  // crashing.
  ASSERT_NE(NULL, glGetString(GL_VERSION));
  const char* ext = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
  if (strstr(ext, "GL_EXT_occlusion_query_boolean")) {
    GLuint a_query = 0;
    glGenQueriesEXT(1, &a_query);
    ASSERT_NE(0, a_query);
    glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, a_query);
    GLboolean is_a_query = glIsQueryEXT(a_query);
    ASSERT_EQ(is_a_query, GL_TRUE);
    glEndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);
    glDeleteQueriesEXT(1, &a_query);
  }
  if (strstr(ext, "GL_ANGLE_instanced_arrays")) {
    glDrawArraysInstancedANGLE(GL_TRIANGLE_STRIP, 0, 0, 0);
  }
  if (strstr(ext, "GL_OES_vertex_array_object")) {
    GLuint a_vertex_array = 0;
    glGenVertexArraysOES(1, &a_vertex_array);
    ASSERT_NE(0, a_vertex_array);
    glBindVertexArrayOES(a_vertex_array);
    GLboolean is_a_vertex_array = glIsVertexArrayOES(a_vertex_array);
    ASSERT_EQ(is_a_vertex_array, GL_TRUE);
    glBindVertexArrayOES(0);
    glDeleteVertexArraysOES(1, &a_vertex_array);
  }
  glSetCurrentContextPPAPI(kInvalidContext);

  int32_t rv = SwapBuffersSync(&context);
  ASSERT_EQ(PP_OK, rv);

  PASS();
}

int32_t TestGraphics3D::SwapBuffersSync(pp::Graphics3D* context) {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(context->SwapBuffers(callback.GetCallback()));
  return callback.result();
}

std::string TestGraphics3D::CheckPixelPPAPI(
    pp::Graphics3D* context,
    int x, int y, const uint8_t expected_color[4]) {
  GLubyte pixel_color[4];
  opengl_es2_->ReadPixels(context->pp_resource(),
      x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel_color);

  ASSERT_EQ(pixel_color[0], expected_color[0]);
  ASSERT_EQ(pixel_color[1], expected_color[1]);
  ASSERT_EQ(pixel_color[2], expected_color[2]);
  ASSERT_EQ(pixel_color[3], expected_color[3]);
  PASS();
}

std::string TestGraphics3D::CheckPixelGL(
    int x, int y, const uint8_t expected_color[4]) {
  GLubyte pixel_color[4];
  glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel_color);

  ASSERT_EQ(pixel_color[0], expected_color[0]);
  ASSERT_EQ(pixel_color[1], expected_color[1]);
  ASSERT_EQ(pixel_color[2], expected_color[2]);
  ASSERT_EQ(pixel_color[3], expected_color[3]);
  PASS();
}

std::string TestGraphics3D::TestBadResource() {
  // The point of this test is mostly just to make sure that we don't crash and
  // provide reasonable (error) results when the resource is bad.
  const PP_Resource kBadResource = 123;

  // Access OpenGLES API through the PPAPI interface.
  opengl_es2_->ClearColor(kBadResource, 0.0f, 0.0f, 0.0f, 0.0f);
  opengl_es2_->Clear(kBadResource, GL_COLOR_BUFFER_BIT);
  ASSERT_EQ(0, opengl_es2_->GetError(kBadResource));
  ASSERT_EQ(NULL, opengl_es2_->GetString(kBadResource, GL_VERSION));
  ASSERT_EQ(-1, opengl_es2_->GetUniformLocation(kBadResource, 0, NULL));
  ASSERT_EQ(GL_FALSE, opengl_es2_->IsBuffer(kBadResource, 0));
  ASSERT_EQ(0, opengl_es2_->CheckFramebufferStatus(kBadResource,
                                                   GL_DRAW_FRAMEBUFFER));

  glSetCurrentContextPPAPI(kBadResource);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  ASSERT_EQ(0, glGetError());
  ASSERT_EQ(NULL, glGetString(GL_VERSION));
  ASSERT_EQ(-1, glGetUniformLocation(0, NULL));
  ASSERT_EQ(GL_FALSE, glIsBuffer(0));
  ASSERT_EQ(0, glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  PASS();
}

