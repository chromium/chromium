// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MOCK_ABSTRACT_TEXTURE_H_
#define GPU_COMMAND_BUFFER_SERVICE_MOCK_ABSTRACT_TEXTURE_H_

#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/service/abstract_texture_android.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {

// SupportsWeakPtr so it's easy to tell when it has been destroyed.
class MockAbstractTexture : public AbstractTextureAndroid,
                            public base::SupportsWeakPtr<MockAbstractTexture> {
 public:
  MockAbstractTexture();
  // If provided, we'll make a TextureBase that returns this id.  We do not
  // delete this texture.
  explicit MockAbstractTexture(GLuint service_id);
  ~MockAbstractTexture() override;

  gpu::TextureBase* GetTextureBase() const override;
  void BindToServiceId(GLuint service_id) override {}
  void NotifyOnContextLost() override {}

 private:
  // May be null.
  std::unique_ptr<gpu::TextureBase> texture_base_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MOCK_ABSTRACT_TEXTURE_H_
