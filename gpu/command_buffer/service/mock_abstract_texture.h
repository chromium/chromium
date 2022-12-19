// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MOCK_ABSTRACT_TEXTURE_H_
#define GPU_COMMAND_BUFFER_SERVICE_MOCK_ABSTRACT_TEXTURE_H_

#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {

// SupportsWeakPtr so it's easy to tell when it has been destroyed.
class MockAbstractTexture
    : public ::testing::NiceMock<gpu::gles2::AbstractTexture>,
      public base::SupportsWeakPtr<MockAbstractTexture> {
 public:
  MockAbstractTexture();
  // If provided, we'll make a TextureBase that returns this id.  We do not
  // delete this texture.
  explicit MockAbstractTexture(GLuint service_id);
  ~MockAbstractTexture() override;

  MOCK_METHOD0(ForceContextLost, void());
  MOCK_CONST_METHOD0(GetTextureBase, gpu::TextureBase*());
  MOCK_METHOD2(SetParameteri, void(GLenum pname, GLint param));
  MOCK_METHOD1(BindToServiceId, void(GLuint service_id));
  MOCK_METHOD0(ReleaseImage, void());
  MOCK_CONST_METHOD0(GetImageForTesting, gl::GLImage*());
  MOCK_METHOD0(SetCleared, void());
  MOCK_METHOD1(SetCleanupCallback, void(CleanupCallback));
  MOCK_METHOD0(NotifyOnContextLost, void());

 private:
  // May be null.
  std::unique_ptr<gpu::TextureBase> texture_base_;
  CleanupCallback cleanup_callback_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MOCK_ABSTRACT_TEXTURE_H_
