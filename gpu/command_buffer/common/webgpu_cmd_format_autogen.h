// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_webgpu_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_AUTOGEN_H_

struct DawnCommands {
  typedef DawnCommands ValueType;
  static const CommandId kCmdId = kDawnCommands;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(uint32_t _trace_id_high,
            uint32_t _trace_id_low,
            uint32_t _commands_shm_id,
            uint32_t _commands_shm_offset,
            uint32_t _size) {
    SetHeader();
    trace_id_high = _trace_id_high;
    trace_id_low = _trace_id_low;
    commands_shm_id = _commands_shm_id;
    commands_shm_offset = _commands_shm_offset;
    size = _size;
  }

  void* Set(void* cmd,
            uint32_t _trace_id_high,
            uint32_t _trace_id_low,
            uint32_t _commands_shm_id,
            uint32_t _commands_shm_offset,
            uint32_t _size) {
    static_cast<ValueType*>(cmd)->Init(_trace_id_high, _trace_id_low,
                                       _commands_shm_id, _commands_shm_offset,
                                       _size);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t trace_id_high;
  uint32_t trace_id_low;
  uint32_t commands_shm_id;
  uint32_t commands_shm_offset;
  uint32_t size;
};

static_assert(sizeof(DawnCommands) == 24, "size of DawnCommands should be 24");
static_assert(offsetof(DawnCommands, header) == 0,
              "offset of DawnCommands header should be 0");
static_assert(offsetof(DawnCommands, trace_id_high) == 4,
              "offset of DawnCommands trace_id_high should be 4");
static_assert(offsetof(DawnCommands, trace_id_low) == 8,
              "offset of DawnCommands trace_id_low should be 8");
static_assert(offsetof(DawnCommands, commands_shm_id) == 12,
              "offset of DawnCommands commands_shm_id should be 12");
static_assert(offsetof(DawnCommands, commands_shm_offset) == 16,
              "offset of DawnCommands commands_shm_offset should be 16");
static_assert(offsetof(DawnCommands, size) == 20,
              "offset of DawnCommands size should be 20");

struct AssociateMailboxImmediate {
  typedef AssociateMailboxImmediate ValueType;
  static const CommandId kCmdId = kAssociateMailboxImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * 1 * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLuint _device_id,
            GLuint _device_generation,
            GLuint _id,
            GLuint _generation,
            uint64_t _usage,
            uint64_t _internal_usage,
            MailboxFlags _flags,
            GLuint _view_format_count,
            GLuint _count,
            const GLuint* _mailbox_and_view_formats) {
    SetHeader(_count);
    device_id = _device_id;
    device_generation = _device_generation;
    id = _id;
    generation = _generation;
    usage = _usage;
    internal_usage = _internal_usage;
    flags = _flags;
    view_format_count = _view_format_count;
    count = _count;
    memcpy(ImmediateDataAddress(this), _mailbox_and_view_formats,
           ComputeDataSize(_count));
  }

  void* Set(void* cmd,
            GLuint _device_id,
            GLuint _device_generation,
            GLuint _id,
            GLuint _generation,
            uint64_t _usage,
            uint64_t _internal_usage,
            MailboxFlags _flags,
            GLuint _view_format_count,
            GLuint _count,
            const GLuint* _mailbox_and_view_formats) {
    static_cast<ValueType*>(cmd)->Init(_device_id, _device_generation, _id,
                                       _generation, _usage, _internal_usage,
                                       _flags, _view_format_count, _count,
                                       _mailbox_and_view_formats);
    const uint32_t size = ComputeSize(_count);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t device_id;
  uint32_t device_generation;
  uint32_t id;
  uint32_t generation;
  uint32_t usage;
  uint32_t internal_usage;
  uint32_t flags;
  uint32_t view_format_count;
  uint32_t count;
};

static_assert(sizeof(AssociateMailboxImmediate) == 40,
              "size of AssociateMailboxImmediate should be 40");
static_assert(offsetof(AssociateMailboxImmediate, header) == 0,
              "offset of AssociateMailboxImmediate header should be 0");
static_assert(offsetof(AssociateMailboxImmediate, device_id) == 4,
              "offset of AssociateMailboxImmediate device_id should be 4");
static_assert(
    offsetof(AssociateMailboxImmediate, device_generation) == 8,
    "offset of AssociateMailboxImmediate device_generation should be 8");
static_assert(offsetof(AssociateMailboxImmediate, id) == 12,
              "offset of AssociateMailboxImmediate id should be 12");
static_assert(offsetof(AssociateMailboxImmediate, generation) == 16,
              "offset of AssociateMailboxImmediate generation should be 16");
static_assert(offsetof(AssociateMailboxImmediate, usage) == 20,
              "offset of AssociateMailboxImmediate usage should be 20");
static_assert(
    offsetof(AssociateMailboxImmediate, internal_usage) == 24,
    "offset of AssociateMailboxImmediate internal_usage should be 24");
static_assert(offsetof(AssociateMailboxImmediate, flags) == 28,
              "offset of AssociateMailboxImmediate flags should be 28");
static_assert(
    offsetof(AssociateMailboxImmediate, view_format_count) == 32,
    "offset of AssociateMailboxImmediate view_format_count should be 32");
static_assert(offsetof(AssociateMailboxImmediate, count) == 36,
              "offset of AssociateMailboxImmediate count should be 36");

struct DissociateMailbox {
  typedef DissociateMailbox ValueType;
  static const CommandId kCmdId = kDissociateMailbox;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture_id, GLuint _texture_generation) {
    SetHeader();
    texture_id = _texture_id;
    texture_generation = _texture_generation;
  }

  void* Set(void* cmd, GLuint _texture_id, GLuint _texture_generation) {
    static_cast<ValueType*>(cmd)->Init(_texture_id, _texture_generation);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture_id;
  uint32_t texture_generation;
};

static_assert(sizeof(DissociateMailbox) == 12,
              "size of DissociateMailbox should be 12");
static_assert(offsetof(DissociateMailbox, header) == 0,
              "offset of DissociateMailbox header should be 0");
static_assert(offsetof(DissociateMailbox, texture_id) == 4,
              "offset of DissociateMailbox texture_id should be 4");
static_assert(offsetof(DissociateMailbox, texture_generation) == 8,
              "offset of DissociateMailbox texture_generation should be 8");

struct DissociateMailboxForPresent {
  typedef DissociateMailboxForPresent ValueType;
  static const CommandId kCmdId = kDissociateMailboxForPresent;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _device_id,
            GLuint _device_generation,
            GLuint _texture_id,
            GLuint _texture_generation) {
    SetHeader();
    device_id = _device_id;
    device_generation = _device_generation;
    texture_id = _texture_id;
    texture_generation = _texture_generation;
  }

  void* Set(void* cmd,
            GLuint _device_id,
            GLuint _device_generation,
            GLuint _texture_id,
            GLuint _texture_generation) {
    static_cast<ValueType*>(cmd)->Init(_device_id, _device_generation,
                                       _texture_id, _texture_generation);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t device_id;
  uint32_t device_generation;
  uint32_t texture_id;
  uint32_t texture_generation;
};

static_assert(sizeof(DissociateMailboxForPresent) == 20,
              "size of DissociateMailboxForPresent should be 20");
static_assert(offsetof(DissociateMailboxForPresent, header) == 0,
              "offset of DissociateMailboxForPresent header should be 0");
static_assert(offsetof(DissociateMailboxForPresent, device_id) == 4,
              "offset of DissociateMailboxForPresent device_id should be 4");
static_assert(
    offsetof(DissociateMailboxForPresent, device_generation) == 8,
    "offset of DissociateMailboxForPresent device_generation should be 8");
static_assert(offsetof(DissociateMailboxForPresent, texture_id) == 12,
              "offset of DissociateMailboxForPresent texture_id should be 12");
static_assert(
    offsetof(DissociateMailboxForPresent, texture_generation) == 16,
    "offset of DissociateMailboxForPresent texture_generation should be 16");

struct SetWebGPUExecutionContextToken {
  typedef SetWebGPUExecutionContextToken ValueType;
  static const CommandId kCmdId = kSetWebGPUExecutionContextToken;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(uint32_t _type,
            uint32_t _high_high,
            uint32_t _high_low,
            uint32_t _low_high,
            uint32_t _low_low) {
    SetHeader();
    type = _type;
    high_high = _high_high;
    high_low = _high_low;
    low_high = _low_high;
    low_low = _low_low;
  }

  void* Set(void* cmd,
            uint32_t _type,
            uint32_t _high_high,
            uint32_t _high_low,
            uint32_t _low_high,
            uint32_t _low_low) {
    static_cast<ValueType*>(cmd)->Init(_type, _high_high, _high_low, _low_high,
                                       _low_low);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t type;
  uint32_t high_high;
  uint32_t high_low;
  uint32_t low_high;
  uint32_t low_low;
};

static_assert(sizeof(SetWebGPUExecutionContextToken) == 24,
              "size of SetWebGPUExecutionContextToken should be 24");
static_assert(offsetof(SetWebGPUExecutionContextToken, header) == 0,
              "offset of SetWebGPUExecutionContextToken header should be 0");
static_assert(offsetof(SetWebGPUExecutionContextToken, type) == 4,
              "offset of SetWebGPUExecutionContextToken type should be 4");
static_assert(offsetof(SetWebGPUExecutionContextToken, high_high) == 8,
              "offset of SetWebGPUExecutionContextToken high_high should be 8");
static_assert(offsetof(SetWebGPUExecutionContextToken, high_low) == 12,
              "offset of SetWebGPUExecutionContextToken high_low should be 12");
static_assert(offsetof(SetWebGPUExecutionContextToken, low_high) == 16,
              "offset of SetWebGPUExecutionContextToken low_high should be 16");
static_assert(offsetof(SetWebGPUExecutionContextToken, low_low) == 20,
              "offset of SetWebGPUExecutionContextToken low_low should be 20");

#endif  // GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_AUTOGEN_H_
