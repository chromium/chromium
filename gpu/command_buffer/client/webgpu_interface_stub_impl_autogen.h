// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_webgpu_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file is included by gles2_interface_stub.cc.
#ifndef GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_STUB_IMPL_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_STUB_IMPL_AUTOGEN_H_

void WebGPUInterfaceStub::DissociateMailbox(GLuint /* texture_id */,
                                            GLuint /* texture_generation */) {}
void WebGPUInterfaceStub::DissociateMailboxForPresent(
    GLuint /* device_id */,
    GLuint /* device_generation */,
    GLuint /* texture_id */,
    GLuint /* texture_generation */) {}
void WebGPUInterfaceStub::SetWebGPUExecutionContextToken(
    uint32_t /* type */,
    uint32_t /* high_high */,
    uint32_t /* high_low */,
    uint32_t /* low_high */,
    uint32_t /* low_low */) {}
#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_INTERFACE_STUB_IMPL_AUTOGEN_H_
