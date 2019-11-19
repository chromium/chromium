// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_webgpu_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file is included by webgpu_implementation.cc to define the
// GL api functions.
#ifndef GPU_COMMAND_BUFFER_CLIENT_WEBGPU_IMPLEMENTATION_IMPL_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_WEBGPU_IMPLEMENTATION_IMPL_AUTOGEN_H_

void WebGPUImplementation::AssociateMailbox(GLuint device_id,
                                            GLuint device_generation,
                                            GLuint id,
                                            GLuint generation,
                                            GLuint usage,
                                            const GLbyte* mailbox) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] wgAssociateMailbox(" << device_id
                     << ", " << device_generation << ", " << id << ", "
                     << generation << ", " << usage << ", "
                     << static_cast<const void*>(mailbox) << ")");
  uint32_t count = 16;
  for (uint32_t ii = 0; ii < count; ++ii)
    GPU_CLIENT_LOG("value[" << ii << "]: " << mailbox[ii]);
  helper_->AssociateMailboxImmediate(device_id, device_generation, id,
                                     generation, usage, mailbox);
  CheckGLError();
}

void WebGPUImplementation::DissociateMailbox(GLuint texture_id,
                                             GLuint texture_generation) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] wgDissociateMailbox(" << texture_id
                     << ", " << texture_generation << ")");
  helper_->DissociateMailbox(texture_id, texture_generation);
}

#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_IMPLEMENTATION_IMPL_AUTOGEN_H_
