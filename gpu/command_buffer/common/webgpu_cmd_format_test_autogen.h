// Copyright 2018 The Chromium Authors
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
              static_cast<uint32_t>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DawnCommands::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32_t>(11), cmd.trace_id_high);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.trace_id_low);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.commands_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.commands_shm_offset);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.size);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(WebGPUFormatTest, AssociateMailboxImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLuint data[] = {
      static_cast<GLuint>(kSomeBaseValueToTestWith + 0),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 1),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 2),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 3),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 4),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 5),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 6),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 7),
      static_cast<GLuint>(kSomeBaseValueToTestWith + 8),
  };
  cmds::AssociateMailboxImmediate& cmd =
      *GetBufferAs<cmds::AssociateMailboxImmediate>();
  const GLsizei kNumElements = 9;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLuint) * 1;
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(1), static_cast<GLuint>(2),
                           static_cast<GLuint>(3), static_cast<GLuint>(4),
                           static_cast<uint64_t>(5), static_cast<uint64_t>(6),
                           static_cast<MailboxFlags>(7), static_cast<GLuint>(8),
                           static_cast<GLuint>(9), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::AssociateMailboxImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(1), cmd.device_id);
  EXPECT_EQ(static_cast<GLuint>(2), cmd.device_generation);
  EXPECT_EQ(static_cast<GLuint>(3), cmd.id);
  EXPECT_EQ(static_cast<GLuint>(4), cmd.generation);
  EXPECT_EQ(static_cast<uint64_t>(5), cmd.usage);
  EXPECT_EQ(static_cast<uint64_t>(6), cmd.internal_usage);
  EXPECT_EQ(static_cast<MailboxFlags>(7), cmd.flags);
  EXPECT_EQ(static_cast<GLuint>(8), cmd.view_format_count);
  EXPECT_EQ(static_cast<GLuint>(9), cmd.count);
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

TEST_F(WebGPUFormatTest, DissociateMailboxForPresent) {
  cmds::DissociateMailboxForPresent& cmd =
      *GetBufferAs<cmds::DissociateMailboxForPresent>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12),
              static_cast<GLuint>(13), static_cast<GLuint>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DissociateMailboxForPresent::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.device_id);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.device_generation);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.texture_id);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.texture_generation);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(WebGPUFormatTest, SetWebGPUExecutionContextToken) {
  cmds::SetWebGPUExecutionContextToken& cmd =
      *GetBufferAs<cmds::SetWebGPUExecutionContextToken>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<uint32_t>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14),
              static_cast<uint32_t>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::SetWebGPUExecutionContextToken::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32_t>(11), cmd.type);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.high_high);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.high_low);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.low_high);
  EXPECT_EQ(static_cast<uint32_t>(15), cmd.low_low);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

#endif  // GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_TEST_AUTOGEN_H_
