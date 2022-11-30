// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_MOCK_H_
#define GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_MOCK_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gl/gl_context_stub.h"

namespace gpu {

class GLContextMock : public gl::GLContextStub {
 public:
  GLContextMock();

  MOCK_METHOD1(MakeCurrentImpl, bool(gl::GLSurface* surface));

 protected:
  ~GLContextMock() override;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_MOCK_H_
