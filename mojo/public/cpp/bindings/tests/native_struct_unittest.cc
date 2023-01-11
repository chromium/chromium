// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "ipc/ipc_param_traits.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/wait.h"
#include "mojo/public/interfaces/bindings/tests/test_native_types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

class NativeStructTest : public BindingsTestBase,
                         public test::NativeTypeTester {
 public:
  NativeStructTest() : receiver_(this, remote_.BindNewPipeAndPassReceiver()) {}

  NativeStructTest(const NativeStructTest&) = delete;
  NativeStructTest& operator=(const NativeStructTest&) = delete;

  ~NativeStructTest() override = default;

  test::NativeTypeTester* remote() { return remote_.get(); }

 private:
  // test::NativeTypeTester:
  void PassNativeStruct(const test::TestNativeStruct& s,
                        PassNativeStructCallback callback) override {
    std::move(callback).Run(s);
  }

  void PassNativeStructWithAttachments(
      test::TestNativeStructWithAttachments s,
      PassNativeStructWithAttachmentsCallback callback) override {
    std::move(callback).Run(std::move(s));
  }

  Remote<test::NativeTypeTester> remote_;
  Receiver<test::NativeTypeTester> receiver_;
};

TEST_P(NativeStructTest, NativeStruct) {
  test::TestNativeStruct s("hello world", 5, 42);
  base::RunLoop loop;
  remote()->PassNativeStruct(
      s, base::BindOnce(
             [](test::TestNativeStruct* expected_struct, base::RunLoop* loop,
                const test::TestNativeStruct& passed) {
               EXPECT_EQ(expected_struct->message(), passed.message());
               EXPECT_EQ(expected_struct->x(), passed.x());
               EXPECT_EQ(expected_struct->y(), passed.y());
               loop->Quit();
             },
             &s, &loop));
  loop.Run();
}

TEST_P(NativeStructTest, NativeStructWithAttachments) {
  mojo::MessagePipe pipe;
  const std::string kTestMessage = "hey hi";
  test::TestNativeStructWithAttachments s(kTestMessage,
                                          std::move(pipe.handle0));
  base::RunLoop loop;
  remote()->PassNativeStructWithAttachments(
      std::move(s),
      base::BindOnce(
          [](const std::string& expected_message,
             mojo::ScopedMessagePipeHandle peer_pipe, base::RunLoop* loop,
             test::TestNativeStructWithAttachments passed) {
            // To ensure that the received pipe handle is functioning, we write
            // to its peer and wait for the message to be received.
            WriteMessageRaw(peer_pipe.get(), "ping", 4, nullptr, 0,
                            MOJO_WRITE_MESSAGE_FLAG_NONE);
            auto pipe = passed.PassPipe();
            EXPECT_EQ(MOJO_RESULT_OK,
                      Wait(pipe.get(), MOJO_HANDLE_SIGNAL_READABLE));
            std::vector<uint8_t> bytes;
            EXPECT_EQ(MOJO_RESULT_OK,
                      ReadMessageRaw(pipe.get(), &bytes, nullptr,
                                     MOJO_READ_MESSAGE_FLAG_NONE));
            EXPECT_EQ("ping", std::string(bytes.begin(), bytes.end()));
            loop->Quit();
          },
          kTestMessage, std::move(pipe.handle1), &loop));
  loop.Run();
}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(NativeStructTest);

}  // namespace mojo
