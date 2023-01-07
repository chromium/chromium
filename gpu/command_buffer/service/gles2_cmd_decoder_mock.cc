// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"

#include "gpu/command_buffer/common/context_creation_attribs.h"

namespace gpu {
namespace gles2 {

MockGLES2Decoder::MockGLES2Decoder(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    Outputter* outputter)
    : GLES2Decoder(client, command_buffer_service, outputter) {
  ON_CALL(*this, MakeCurrent())
      .WillByDefault(testing::Return(true));
}

MockGLES2Decoder::~MockGLES2Decoder() = default;

base::WeakPtr<DecoderContext> MockGLES2Decoder::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace gles2
}  // namespace gpu
