// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_GLES2_DECODER_HELPER_H_
#define MEDIA_GPU_GLES2_DECODER_HELPER_H_

#include <stdint.h>

#include <memory>

#include "media/gpu/media_gpu_export.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
class DecoderContext;
}  // namespace gpu

namespace media {

// Utility methods to simplify working with a gpu::DecoderContext from
// inside VDAs.
class MEDIA_GPU_EXPORT GLES2DecoderHelper {
 public:
  static std::unique_ptr<GLES2DecoderHelper> Create(
      gpu::DecoderContext* decoder);

  virtual ~GLES2DecoderHelper() {}

  // TODO(sandersd): Provide scoped version?
  virtual bool MakeContextCurrent() = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_GLES2_DECODER_HELPER_H_
