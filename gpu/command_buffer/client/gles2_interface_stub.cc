// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gles2_interface_stub.h"

namespace gpu {
namespace gles2 {

GLES2InterfaceStub::GLES2InterfaceStub() = default;

GLES2InterfaceStub::~GLES2InterfaceStub() = default;

// InterfaceBase implementation.
void GLES2InterfaceStub::GenSyncTokenCHROMIUM(GLbyte* sync_token) {}
void GLES2InterfaceStub::GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) {}
void GLES2InterfaceStub::VerifySyncTokensCHROMIUM(GLbyte** sync_tokens,
                                                  GLsizei count) {}
void GLES2InterfaceStub::WaitSyncTokenCHROMIUM(const GLbyte* sync_token) {}
void GLES2InterfaceStub::ShallowFlushCHROMIUM() {}

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/gles2_interface_stub_impl_autogen.h"

}  // namespace gles2
}  // namespace gpu


