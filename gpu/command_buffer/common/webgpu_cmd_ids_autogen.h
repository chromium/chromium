// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_webgpu_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_IDS_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_IDS_AUTOGEN_H_

#define WEBGPU_COMMAND_LIST(OP)           \
  OP(DawnCommands)              /* 256 */ \
  OP(AssociateMailboxImmediate) /* 257 */ \
  OP(DissociateMailbox)         /* 258 */ \
  OP(RequestAdapter)            /* 259 */ \
  OP(RequestDevice)             /* 260 */

enum CommandId {
  kOneBeforeStartPoint =
      cmd::kLastCommonId,  // All WebGPU commands start after this.
#define WEBGPU_CMD_OP(name) k##name,
  WEBGPU_COMMAND_LIST(WEBGPU_CMD_OP)
#undef WEBGPU_CMD_OP
      kNumCommands,
  kFirstWebGPUCommand = kOneBeforeStartPoint + 1
};

#endif  // GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_IDS_AUTOGEN_H_
