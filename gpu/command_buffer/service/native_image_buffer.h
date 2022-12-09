// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_NATIVE_IMAGE_BUFFER_H_
#define GPU_COMMAND_BUFFER_SERVICE_NATIVE_IMAGE_BUFFER_H_

#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/gl_utils.h"

namespace gpu {
namespace gles2 {

class NativeImageBuffer : public base::RefCountedThreadSafe<NativeImageBuffer> {
 public:
  static scoped_refptr<NativeImageBuffer> Create(GLuint texture_id);

  NativeImageBuffer(const NativeImageBuffer&) = delete;
  NativeImageBuffer& operator=(const NativeImageBuffer&) = delete;

  virtual void BindToTexture(GLenum target) const = 0;

 protected:
  friend class base::RefCountedThreadSafe<NativeImageBuffer>;
  NativeImageBuffer() = default;
  virtual ~NativeImageBuffer() = default;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_NATIVE_IMAGE_BUFFER_H_
