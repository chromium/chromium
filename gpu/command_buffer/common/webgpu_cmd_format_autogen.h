// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_webgpu_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_AUTOGEN_H_

#define GL_SCANOUT_CHROMIUM 0x6000

struct DawnCommands {
  typedef DawnCommands ValueType;
  static const CommandId kCmdId = kDawnCommands;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(uint32_t _commands_shm_id,
            uint32_t _commands_shm_offset,
            uint32_t _size) {
    SetHeader();
    commands_shm_id = _commands_shm_id;
    commands_shm_offset = _commands_shm_offset;
    size = _size;
  }

  void* Set(void* cmd,
            uint32_t _commands_shm_id,
            uint32_t _commands_shm_offset,
            uint32_t _size) {
    static_cast<ValueType*>(cmd)->Init(_commands_shm_id, _commands_shm_offset,
                                       _size);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t commands_shm_id;
  uint32_t commands_shm_offset;
  uint32_t size;
};

static_assert(sizeof(DawnCommands) == 16, "size of DawnCommands should be 16");
static_assert(offsetof(DawnCommands, header) == 0,
              "offset of DawnCommands header should be 0");
static_assert(offsetof(DawnCommands, commands_shm_id) == 4,
              "offset of DawnCommands commands_shm_id should be 4");
static_assert(offsetof(DawnCommands, commands_shm_offset) == 8,
              "offset of DawnCommands commands_shm_offset should be 8");
static_assert(offsetof(DawnCommands, size) == 12,
              "offset of DawnCommands size should be 12");

struct AssociateMailboxImmediate {
  typedef AssociateMailboxImmediate ValueType;
  static const CommandId kCmdId = kAssociateMailboxImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLbyte) * 16);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _device_id,
            GLuint _device_generation,
            GLuint _id,
            GLuint _generation,
            GLuint _usage,
            const GLbyte* _mailbox) {
    SetHeader();
    device_id = _device_id;
    device_generation = _device_generation;
    id = _id;
    generation = _generation;
    usage = _usage;
    memcpy(ImmediateDataAddress(this), _mailbox, ComputeDataSize());
  }

  void* Set(void* cmd,
            GLuint _device_id,
            GLuint _device_generation,
            GLuint _id,
            GLuint _generation,
            GLuint _usage,
            const GLbyte* _mailbox) {
    static_cast<ValueType*>(cmd)->Init(_device_id, _device_generation, _id,
                                       _generation, _usage, _mailbox);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t device_id;
  uint32_t device_generation;
  uint32_t id;
  uint32_t generation;
  uint32_t usage;
};

static_assert(sizeof(AssociateMailboxImmediate) == 24,
              "size of AssociateMailboxImmediate should be 24");
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

struct RequestAdapter {
  typedef RequestAdapter ValueType;
  static const CommandId kCmdId = kRequestAdapter;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(uint64_t _request_adapter_serial, uint32_t _power_preference) {
    SetHeader();
    request_adapter_serial = _request_adapter_serial;
    power_preference = _power_preference;
  }

  void* Set(void* cmd,
            uint64_t _request_adapter_serial,
            uint32_t _power_preference) {
    static_cast<ValueType*>(cmd)->Init(_request_adapter_serial,
                                       _power_preference);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t request_adapter_serial;
  uint32_t power_preference;
};

static_assert(sizeof(RequestAdapter) == 12,
              "size of RequestAdapter should be 12");
static_assert(offsetof(RequestAdapter, header) == 0,
              "offset of RequestAdapter header should be 0");
static_assert(offsetof(RequestAdapter, request_adapter_serial) == 4,
              "offset of RequestAdapter request_adapter_serial should be 4");
static_assert(offsetof(RequestAdapter, power_preference) == 8,
              "offset of RequestAdapter power_preference should be 8");

struct RequestDevice {
  typedef RequestDevice ValueType;
  static const CommandId kCmdId = kRequestDevice;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(uint64_t _request_device_serial,
            uint32_t _adapter_service_id,
            uint32_t _device_id,
            uint32_t _device_generation,
            uint32_t _request_device_properties_shm_id,
            uint32_t _request_device_properties_shm_offset,
            uint32_t _request_device_properties_size) {
    SetHeader();
    request_device_serial = _request_device_serial;
    adapter_service_id = _adapter_service_id;
    device_id = _device_id;
    device_generation = _device_generation;
    request_device_properties_shm_id = _request_device_properties_shm_id;
    request_device_properties_shm_offset =
        _request_device_properties_shm_offset;
    request_device_properties_size = _request_device_properties_size;
  }

  void* Set(void* cmd,
            uint64_t _request_device_serial,
            uint32_t _adapter_service_id,
            uint32_t _device_id,
            uint32_t _device_generation,
            uint32_t _request_device_properties_shm_id,
            uint32_t _request_device_properties_shm_offset,
            uint32_t _request_device_properties_size) {
    static_cast<ValueType*>(cmd)->Init(
        _request_device_serial, _adapter_service_id, _device_id,
        _device_generation, _request_device_properties_shm_id,
        _request_device_properties_shm_offset, _request_device_properties_size);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t request_device_serial;
  uint32_t adapter_service_id;
  uint32_t device_id;
  uint32_t device_generation;
  uint32_t request_device_properties_shm_id;
  uint32_t request_device_properties_shm_offset;
  uint32_t request_device_properties_size;
};

static_assert(sizeof(RequestDevice) == 32,
              "size of RequestDevice should be 32");
static_assert(offsetof(RequestDevice, header) == 0,
              "offset of RequestDevice header should be 0");
static_assert(offsetof(RequestDevice, request_device_serial) == 4,
              "offset of RequestDevice request_device_serial should be 4");
static_assert(offsetof(RequestDevice, adapter_service_id) == 8,
              "offset of RequestDevice adapter_service_id should be 8");
static_assert(offsetof(RequestDevice, device_id) == 12,
              "offset of RequestDevice device_id should be 12");
static_assert(offsetof(RequestDevice, device_generation) == 16,
              "offset of RequestDevice device_generation should be 16");
static_assert(
    offsetof(RequestDevice, request_device_properties_shm_id) == 20,
    "offset of RequestDevice request_device_properties_shm_id should be 20");
static_assert(offsetof(RequestDevice, request_device_properties_shm_offset) ==
                  24,
              "offset of RequestDevice request_device_properties_shm_offset "
              "should be 24");
static_assert(
    offsetof(RequestDevice, request_device_properties_size) == 28,
    "offset of RequestDevice request_device_properties_size should be 28");

#endif  // GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_AUTOGEN_H_
