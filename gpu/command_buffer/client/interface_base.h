// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_INTERFACE_BASE_H_
#define GPU_COMMAND_BUFFER_CLIENT_INTERFACE_BASE_H_

#include <GLES2/gl2.h>

namespace gpu {

class InterfaceBase {
 public:
  virtual void GenSyncTokenCHROMIUM(GLbyte* sync_token) = 0;
  virtual void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) = 0;
  virtual void VerifySyncTokensCHROMIUM(GLbyte** sync_tokens,
                                        GLsizei count) = 0;
  virtual void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) = 0;
  virtual void ShallowFlushCHROMIUM() = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_INTERFACE_BASE_H_
