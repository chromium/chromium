// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_webgpu_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file contains unit tests for webgpu commands
// It is included by webgpu_cmd_format_test.cc

#ifndef GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_TEST_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_TEST_AUTOGEN_H_

TEST_F(WebGPUFormatTest, DawnCommands) {
  cmds::DawnCommands& cmd = *GetBufferAs<cmds::DawnCommands>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<uint32_t>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DawnCommands::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32_t>(11), cmd.commands_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.commands_shm_offset);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.size);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(WebGPUFormatTest, AssociateMailboxImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
  };
  cmds::AssociateMailboxImmediate& cmd =
      *GetBufferAs<cmds::AssociateMailboxImmediate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12),
              static_cast<GLuint>(13), static_cast<GLuint>(14),
              static_cast<GLuint>(15), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::AssociateMailboxImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.device_id);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.device_generation);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.id);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.generation);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.usage);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(WebGPUFormatTest, DissociateMailbox) {
  cmds::DissociateMailbox& cmd = *GetBufferAs<cmds::DissociateMailbox>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DissociateMailbox::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture_id);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.texture_generation);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(WebGPUFormatTest, RequestAdapter) {
  cmds::RequestAdapter& cmd = *GetBufferAs<cmds::RequestAdapter>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<uint64_t>(11), static_cast<uint32_t>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::RequestAdapter::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint64_t>(11), cmd.request_adapter_serial);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.power_preference);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(WebGPUFormatTest, RequestDevice) {
  cmds::RequestDevice& cmd = *GetBufferAs<cmds::RequestDevice>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<uint64_t>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15), static_cast<uint32_t>(16),
              static_cast<uint32_t>(17));
  EXPECT_EQ(static_cast<uint32_t>(cmds::RequestDevice::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint64_t>(11), cmd.request_device_serial);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.adapter_service_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.device_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.device_generation);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.request_device_properties_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(16),
            cmd.request_device_properties_shm_offset);
  EXPECT_EQ(static_cast<uint32_t>(17), cmd.request_device_properties_size);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

#endif  // GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_TEST_AUTOGEN_H_
