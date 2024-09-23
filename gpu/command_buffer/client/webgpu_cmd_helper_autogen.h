// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_webgpu_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_CLIENT_WEBGPU_CMD_HELPER_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_WEBGPU_CMD_HELPER_AUTOGEN_H_

void DawnCommands(uint32_t trace_id_high,
                  uint32_t trace_id_low,
                  uint32_t commands_shm_id,
                  uint32_t commands_shm_offset,
                  uint32_t size) {
  webgpu::cmds::DawnCommands* c = GetCmdSpace<webgpu::cmds::DawnCommands>();
  if (c) {
    c->Init(trace_id_high, trace_id_low, commands_shm_id, commands_shm_offset,
            size);
  }
}

void AssociateMailboxImmediate(GLuint device_id,
                               GLuint device_generation,
                               GLuint id,
                               GLuint generation,
                               uint64_t usage,
                               uint64_t internal_usage,
                               MailboxFlags flags,
                               GLuint view_format_count,
                               GLuint count,
                               const GLuint* mailbox_and_view_formats) {
  const uint32_t size =
      webgpu::cmds::AssociateMailboxImmediate::ComputeSize(count);
  webgpu::cmds::AssociateMailboxImmediate* c =
      GetImmediateCmdSpaceTotalSize<webgpu::cmds::AssociateMailboxImmediate>(
          size);
  if (c) {
    c->Init(device_id, device_generation, id, generation, usage, internal_usage,
            flags, view_format_count, count, mailbox_and_view_formats);
  }
}

void DissociateMailbox(GLuint texture_id, GLuint texture_generation) {
  webgpu::cmds::DissociateMailbox* c =
      GetCmdSpace<webgpu::cmds::DissociateMailbox>();
  if (c) {
    c->Init(texture_id, texture_generation);
  }
}

void DissociateMailboxForPresent(GLuint device_id,
                                 GLuint device_generation,
                                 GLuint texture_id,
                                 GLuint texture_generation) {
  webgpu::cmds::DissociateMailboxForPresent* c =
      GetCmdSpace<webgpu::cmds::DissociateMailboxForPresent>();
  if (c) {
    c->Init(device_id, device_generation, texture_id, texture_generation);
  }
}

void SetWebGPUExecutionContextToken(uint32_t type,
                                    uint32_t high_high,
                                    uint32_t high_low,
                                    uint32_t low_high,
                                    uint32_t low_low) {
  webgpu::cmds::SetWebGPUExecutionContextToken* c =
      GetCmdSpace<webgpu::cmds::SetWebGPUExecutionContextToken>();
  if (c) {
    c->Init(type, high_high, high_low, low_high, low_low);
  }
}

#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_CMD_HELPER_AUTOGEN_H_
