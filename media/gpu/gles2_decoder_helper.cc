// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/gles2_decoder_helper.h"

#include <memory>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "ui/gl/gl_context.h"

namespace media {

class GLES2DecoderHelperImpl : public GLES2DecoderHelper {
 public:
  explicit GLES2DecoderHelperImpl(gpu::DecoderContext* decoder)
      : decoder_(decoder) {
    DCHECK(decoder_);
  }

  GLES2DecoderHelperImpl(const GLES2DecoderHelperImpl&) = delete;
  GLES2DecoderHelperImpl& operator=(const GLES2DecoderHelperImpl&) = delete;

  bool MakeContextCurrent() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return decoder_->MakeCurrent();
  }

 private:
  raw_ptr<gpu::DecoderContext> decoder_;
  THREAD_CHECKER(thread_checker_);
};

// static
std::unique_ptr<GLES2DecoderHelper> GLES2DecoderHelper::Create(
    gpu::DecoderContext* decoder) {
  return std::make_unique<GLES2DecoderHelperImpl>(decoder);
}

}  // namespace media
