// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>
#include <vector>

#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

constexpr int32_t kTestMessageName = 42;
constexpr int32_t kTestMessageFlags = 7;
constexpr uint32_t kTestPayloadSize = 32;

void CreateTestMessagePayload(std::vector<uint8_t>* bytes,
                              std::vector<ScopedHandle>* handles) {
  Message message(kTestMessageName, kTestMessageFlags, 0, kTestPayloadSize,
                  nullptr);
  message.header()->trace_id = 0;
  bytes->resize(message.data_num_bytes());
  std::copy(message.data(), message.data() + message.data_num_bytes(),
            bytes->begin());

  MessagePipe pipe;
  handles->resize(2);
  handles->at(0) = ScopedHandle(std::move(pipe.handle0));
  handles->at(1) = ScopedHandle(std::move(pipe.handle1));
}

TEST(BindingsMessageTest, ConstructFromPayload) {
  // Verifies that Message objects constructed directly from a raw payload look
  // the same on the wire as raw messages constructed with lower level APIs.
  MessagePipe pipe;

  // First feed the raw message data directly into the pipe.
  std::vector<uint8_t> in_bytes1;
  std::vector<ScopedHandle> in_handles1;
  CreateTestMessagePayload(&in_bytes1, &in_handles1);
  WriteMessageRaw(pipe.handle0.get(), in_bytes1.data(), in_bytes1.size(),
                  reinterpret_cast<const MojoHandle*>(in_handles1.data()),
                  in_handles1.size(), MOJO_WRITE_MESSAGE_FLAG_NONE);
  for (auto& handle : in_handles1)
    ignore_result(handle.release());

  // Now construct a Message object from the same payload and feed that into the
  // pipe.
  std::vector<uint8_t> in_bytes2;
  std::vector<ScopedHandle> in_handles2;
  CreateTestMessagePayload(&in_bytes2, &in_handles2);
  Message message(in_bytes2, in_handles2);
  WriteMessageNew(pipe.handle0.get(), message.TakeMojoMessage(),
                  MOJO_WRITE_MESSAGE_FLAG_NONE);

  // Now read both messages and ensure that they're identical.
  // NOTE: The handles themselves cannot be identical, but the same number of
  // handles should be attached.
  std::vector<uint8_t> out_bytes1;
  std::vector<ScopedHandle> out_handles1;
  ASSERT_EQ(MOJO_RESULT_OK,
            ReadMessageRaw(pipe.handle1.get(), &out_bytes1, &out_handles1,
                           MOJO_READ_MESSAGE_FLAG_NONE));
  std::vector<uint8_t> out_bytes2;
  std::vector<ScopedHandle> out_handles2;
  ASSERT_EQ(MOJO_RESULT_OK,
            ReadMessageRaw(pipe.handle1.get(), &out_bytes2, &out_handles2,
                           MOJO_READ_MESSAGE_FLAG_NONE));

  EXPECT_EQ(out_bytes1, out_bytes2);
  EXPECT_EQ(out_handles1.size(), out_handles2.size());
}

}  // namespace
}  // namespace test
}  // namespace mojo
