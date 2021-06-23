// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_GL_STRING_QUERY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_GL_STRING_QUERY_H_

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Performs a query for a string on GLES2Interface and returns a WTF::String. If
// it is unable to query the string, it wil return an empty WTF::String.
class GLStringQuery {
 public:
  struct ProgramInfoLog {
    static void LengthFunction(gpu::gles2::GLES2Interface* gl,
                               GLuint id,
                               GLint* length) {
      return gl->GetProgramiv(id, GL_INFO_LOG_LENGTH, length);
    }
    static void LogFunction(gpu::gles2::GLES2Interface* gl,
                            GLuint id,
                            GLint length,
                            GLint* returned_length,
                            LChar* ptr) {
      return gl->GetProgramInfoLog(id, length, returned_length,
                                   reinterpret_cast<GLchar*>(ptr));
    }
  };

  struct ShaderInfoLog {
    static void LengthFunction(gpu::gles2::GLES2Interface* gl,
                               GLuint id,
                               GLint* length) {
      gl->GetShaderiv(id, GL_INFO_LOG_LENGTH, length);
    }
    static void LogFunction(gpu::gles2::GLES2Interface* gl,
                            GLuint id,
                            GLint length,
                            GLint* returned_length,
                            LChar* ptr) {
      gl->GetShaderInfoLog(id, length, returned_length,
                           reinterpret_cast<GLchar*>(ptr));
    }
  };

  struct TranslatedShaderSourceANGLE {
    static void LengthFunction(gpu::gles2::GLES2Interface* gl,
                               GLuint id,
                               GLint* length) {
      gl->GetShaderiv(id, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE, length);
    }
    static void LogFunction(gpu::gles2::GLES2Interface* gl,
                            GLuint id,
                            GLint length,
                            GLint* returned_length,
                            LChar* ptr) {
      gl->GetTranslatedShaderSourceANGLE(id, length, returned_length,
                                         reinterpret_cast<GLchar*>(ptr));
    }
  };

  GLStringQuery(gpu::gles2::GLES2Interface* gl) : gl_(gl) {}

  template <class Traits>
  WTF::String Run(GLuint id) {
    GLint length = 0;
    Traits::LengthFunction(gl_, id, &length);
    if (!length)
      return WTF::g_empty_string;
    LChar* log_ptr;
    scoped_refptr<WTF::StringImpl> name_impl =
        WTF::StringImpl::CreateUninitialized(length, log_ptr);
    GLsizei returned_length = 0;
    Traits::LogFunction(gl_, id, length, &returned_length, log_ptr);
    // The returnedLength excludes the null terminator. If this check wasn't
    // true, then we'd need to tell the returned String the real length.
    DCHECK_EQ(returned_length + 1, length);
    return String(std::move(name_impl));
  }

 private:
  gpu::gles2::GLES2Interface* gl_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_GL_STRING_QUERY_H_
