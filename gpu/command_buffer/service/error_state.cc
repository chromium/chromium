// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/error_state.h"

#include <stdint.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/logger.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
namespace gles2 {
namespace {
GLenum GetErrorHelper() {
  // Skip calling glGetError if no context is bound - this should only happen
  // when GL is not used e.g. with Graphite.
  if (gl::g_current_gl_driver) {
    gl::GLApi* const api = gl::g_current_gl_context;
    return api->glGetErrorFn();
  }
  return GL_NO_ERROR;
}
}  // namespace

class ErrorStateImpl : public ErrorState {
 public:
  explicit ErrorStateImpl(ErrorStateClient* client, Logger* logger);

  ErrorStateImpl(const ErrorStateImpl&) = delete;
  ErrorStateImpl& operator=(const ErrorStateImpl&) = delete;

  ~ErrorStateImpl() override;

  uint32_t GetGLError() override;

  void SetGLError(const char* filename,
                  int line,
                  unsigned int error,
                  const char* function_name,
                  const char* msg) override;
  void SetGLErrorInvalidEnum(const char* filename,
                             int line,
                             const char* function_name,
                             unsigned int value,
                             const char* label) override;
  void SetGLErrorInvalidParami(const char* filename,
                               int line,
                               unsigned int error,
                               const char* function_name,
                               unsigned int pname,
                               int param) override;
  void SetGLErrorInvalidParamf(const char* filename,
                               int line,
                               unsigned int error,
                               const char* function_name,
                               unsigned int pname,
                               float param) override;

  unsigned int PeekGLError(const char* filename,
                           int line,
                           const char* function_name) override;

  void CopyRealGLErrorsToWrapper(const char* filename,
                                 int line,
                                 const char* function_name) override;

  void ClearRealGLErrors(const char* filename,
                         int line,
                         const char* function_name) override;

 private:
  GLenum GetErrorHandleContextLoss();

  // The last error message set.
  std::string last_error_;
  // Current GL error bits.
  uint32_t error_bits_;

  raw_ptr<ErrorStateClient> client_;
  raw_ptr<Logger> logger_;
};

ErrorState::ErrorState() = default;

ErrorState::~ErrorState() = default;

ErrorState* ErrorState::Create(ErrorStateClient* client, Logger* logger) {
  return new ErrorStateImpl(client, logger);
}

ErrorStateImpl::ErrorStateImpl(ErrorStateClient* client, Logger* logger)
    : error_bits_(0), client_(client), logger_(logger) {}

ErrorStateImpl::~ErrorStateImpl() = default;

uint32_t ErrorStateImpl::GetGLError() {
  // Check the GL error first, then our wrapped error.
  GLenum error = GetErrorHandleContextLoss();
  if (error == GL_NO_ERROR && error_bits_ != 0) {
    for (uint32_t mask = 1; mask != 0; mask = mask << 1) {
      if ((error_bits_ & mask) != 0) {
        error = GLES2Util::GLErrorBitToGLError(mask);
        break;
      }
    }
  }

  if (error != GL_NO_ERROR) {
    // There was an error, clear the corresponding wrapped error.
    error_bits_ &= ~GLES2Util::GLErrorToErrorBit(error);
  }
  return error;
}

GLenum ErrorStateImpl::GetErrorHandleContextLoss() {
  GLenum error = GetErrorHelper();
  if (error == GL_CONTEXT_LOST_KHR) {
    client_->OnContextLostError();
    // Do not expose GL_CONTEXT_LOST_KHR, as the version of the robustness
    // extension that introduces the error is not exposed by the command
    // buffer.
    error = GL_NO_ERROR;
  }
  return error;
}

unsigned int ErrorStateImpl::PeekGLError(
    const char* filename, int line, const char* function_name) {
  GLenum error = GetErrorHandleContextLoss();
  if (error != GL_NO_ERROR) {
    SetGLError(filename, line, error, function_name, "");
  }
  return error;
}

void ErrorStateImpl::SetGLError(
    const char* filename,
    int line,
    unsigned int error,
    const char* function_name,
    const char* msg) {
  if (msg) {
    last_error_ = msg;
    logger_->LogMessage(
        filename, line,
        std::string("GL ERROR :") +
        GLES2Util::GetStringEnum(error) + " : " +
        function_name + ": " + msg);
  }
  error_bits_ |= GLES2Util::GLErrorToErrorBit(error);
  if (error == GL_OUT_OF_MEMORY)
    client_->OnOutOfMemoryError();
}

void ErrorStateImpl::SetGLErrorInvalidEnum(
    const char* filename,
    int line,
    const char* function_name,
    unsigned int value,
    const char* label) {
  SetGLError(filename, line, GL_INVALID_ENUM, function_name,
             (std::string(label) + " was " +
             GLES2Util::GetStringEnum(value)).c_str());
}

void ErrorStateImpl::SetGLErrorInvalidParami(
    const char* filename,
    int line,
    unsigned int error,
    const char* function_name,
    unsigned int pname, int param) {
  if (error == GL_INVALID_ENUM) {
    SetGLError(
        filename, line, GL_INVALID_ENUM, function_name,
        (std::string("trying to set ") +
         GLES2Util::GetStringEnum(pname) + " to " +
         GLES2Util::GetStringEnum(param)).c_str());
  } else {
    SetGLError(
        filename, line, error, function_name,
        (std::string("trying to set ") + GLES2Util::GetStringEnum(pname) +
         " to " + base::NumberToString(param))
            .c_str());
  }
}

void ErrorStateImpl::SetGLErrorInvalidParamf(
    const char* filename,
    int line,
    unsigned int error,
    const char* function_name,
    unsigned int pname, float param) {
  SetGLError(
      filename, line, error, function_name,
      (std::string("trying to set ") +
       GLES2Util::GetStringEnum(pname) + " to " +
       base::StringPrintf("%G", param)).c_str());
}

void ErrorStateImpl::CopyRealGLErrorsToWrapper(
    const char* filename, int line, const char* function_name) {
  GLenum error;
  while ((error = GetErrorHandleContextLoss()) != GL_NO_ERROR) {
    SetGLError(filename, line, error, function_name,
               "<- error from previous GL command");
  }
}

void ErrorStateImpl::ClearRealGLErrors(
    const char* filename, int line, const char* function_name) {
  // Clears and logs all current gl errors.
  GLenum error;
  while ((error = GetErrorHelper()) != GL_NO_ERROR) {
    if (error != GL_CONTEXT_LOST_KHR && error != GL_OUT_OF_MEMORY) {
      // GL_OUT_OF_MEMORY can legally happen on lost device.
      logger_->LogMessage(
          filename, line,
          std::string("GL ERROR :") +
          GLES2Util::GetStringEnum(error) + " : " +
          function_name + ": was unhandled");
      NOTREACHED_IN_MIGRATION() << "GL error " << error << " was unhandled.";
    }
  }
}

}  // namespace gles2
}  // namespace gpu
