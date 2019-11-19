// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper functions for GL.

#ifndef GPU_COMMAND_BUFFER_TESTS_GL_TEST_UTILS_H_
#define GPU_COMMAND_BUFFER_TESTS_GL_TEST_UTILS_H_

#include <GLES2/gl2.h>
#include <stdint.h>

#include <vector>

#include "build/build_config.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "ui/gl/gl_implementation.h"

namespace gl {
class GLImageNativePixmap;
}

namespace gpu {

class GLTestHelper {
 public:
  static const uint8_t kCheckClearValue = 123u;

  static bool InitializeGL(gl::GLImplementation gl_impl);
  static bool InitializeGLDefault();

  static bool HasExtension(const char* extension);
  static bool CheckGLError(const char* msg, int line);

  // Compiles a shader.
  // Does not check for errors, always returns shader.
  static GLuint CompileShader(GLenum type, const char* shaderSrc);

  // Compiles a shader and checks for compilation errors.
  // Returns shader, 0 on failure.
  static GLuint LoadShader(GLenum type, const char* shaderSrc);

  // Attaches 2 shaders and links them to a program.
  // Does not check for errors, always returns program.
  static GLuint LinkProgram(GLuint vertex_shader, GLuint fragment_shader);

  // Attaches 2 shaders, links them to a program, and checks for errors.
  // Returns program, 0 on failure.
  static GLuint SetupProgram(GLuint vertex_shader, GLuint fragment_shader);

  // Compiles 2 shaders, attaches and links them to a program
  // Returns program, 0 on failure.
  static GLuint LoadProgram(
      const char* vertex_shader_source,
      const char* fragment_shader_source);

  // Make a unit quad with position only.
  // Returns the created buffer.
  static GLuint SetupUnitQuad(GLint position_location);

  // Returns a vector of size 2. The first is the array buffer object,
  // the second is the element array buffer object.
  static std::vector<GLuint> SetupIndexedUnitQuad(GLint position_location);

  // Make a 6 vertex colors.
  // Returns the created buffer.
  static GLuint SetupColorsForUnitQuad(
      GLint location, const GLfloat color[4], GLenum usage);

  // Checks an area of pixels for a color.
  // If mask is nullptr, compare all color channels; otherwise, compare the
  // channels whose corresponding mask bit is true.
  static bool CheckPixels(GLint x,
                          GLint y,
                          GLsizei width,
                          GLsizei height,
                          GLint tolerance,
                          const uint8_t* color,
                          const uint8_t* mask);

  static bool CheckPixels(GLint x,
                          GLint y,
                          GLsizei width,
                          GLsizei height,
                          GLint tolerance,
                          const std::vector<uint8_t>& expected,
                          const uint8_t* mask);

  // Uses ReadPixels to save an area of the current FBO/Backbuffer.
  static bool SaveBackbufferAsBMP(const char* filename, int width, int height);

  static void DrawTextureQuad(const GLenum texture_target,
                              const char* vertex_src,
                              const char* fragment_src,
                              const char* position_name,
                              const char* sampler_name,
                              const char* face_name);
};

class GpuCommandBufferTestEGL {
 public:
  GpuCommandBufferTestEGL();
  ~GpuCommandBufferTestEGL();

  // Reinitialize GL to the EGLGLES2 implementation if it is available and not
  // the current initialized GL implementation. Return true on sucess, false
  // otherwise.
  bool InitializeEGLGLES2(int width, int height);

  // Restore the default GL implementation.
  void RestoreGLDefault();

  // Returns whether the current context supports the named EGL extension.
  bool HasEGLExtension(const base::StringPiece& extension) {
    return gfx::HasExtension(egl_extensions_, extension);
  }

  // Returns whether the current context supports the named GL extension.
  bool HasGLExtension(const base::StringPiece& extension) {
    return gfx::HasExtension(gl_extensions_, extension);
  }

#if defined(OS_LINUX)
  // Create GLImageNativePixmap filled in with the given pixels.
  scoped_refptr<gl::GLImageNativePixmap> CreateGLImageNativePixmap(
      gfx::BufferFormat format,
      gfx::Size size,
      uint8_t* pixels) const;

  // Get some real dmabuf fds for testing by exporting an EGLImage created from
  // a GL texture.
  gfx::NativePixmapHandle CreateNativePixmapHandle(gfx::BufferFormat format,
                                                   gfx::Size size,
                                                   uint8_t* pixels);
#endif

 protected:
  bool gl_reinitialized_;
  GLManager gl_;
  gl::GLWindowSystemBindingInfo window_system_binding_info_;
  gfx::ExtensionSet egl_extensions_;
  gfx::ExtensionSet gl_extensions_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_TESTS_GL_TEST_UTILS_H_
