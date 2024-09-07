// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/host/native_messaging/native_messaging_reader.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "remoting/host/setup/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class NativeMessagingReaderTest : public testing::Test {
 public:
  NativeMessagingReaderTest();
  ~NativeMessagingReaderTest() override;

  void SetUp() override;

  // Runs the MessageLoop to completion.
  void RunAndWaitForOperationComplete();

  // MessageCallback passed to the Reader. Stores |message| so it can be
  // verified by tests.
  void OnMessage(base::Value message);

  // Closure passed to the Reader, called back when the reader detects an error.
  void OnError();

  // Writes a message (header+body) to the write-end of the pipe.
  void WriteMessage(const std::string& message);

  // Writes some data to the write-end of the pipe.
  void WriteData(const char* data, int length);

 protected:
  std::unique_ptr<NativeMessagingReader> reader_;
  base::File read_file_;
  base::File write_file_;
  bool on_error_signaled_ = false;
  std::optional<base::Value> message_;

 private:
  // MessageLoop declared here, since the NativeMessageReader ctor requires a
  // MessageLoop to have been created.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<base::RunLoop> run_loop_;
};

NativeMessagingReaderTest::NativeMessagingReaderTest() = default;

NativeMessagingReaderTest::~NativeMessagingReaderTest() = default;

void NativeMessagingReaderTest::SetUp() {
  ASSERT_TRUE(MakePipe(&read_file_, &write_file_));
  reader_ = std::make_unique<NativeMessagingReader>(std::move(read_file_));
  run_loop_ = std::make_unique<base::RunLoop>();

  // base::Unretained is safe since no further tasks can run after
  // RunLoop::Run() returns.
  reader_->Start(base::BindRepeating(&NativeMessagingReaderTest::OnMessage,
                                     base::Unretained(this)),
                 base::BindOnce(&NativeMessagingReaderTest::OnError,
                                base::Unretained(this)));
}

void NativeMessagingReaderTest::RunAndWaitForOperationComplete() {
  run_loop_->Run();
  run_loop_ = std::make_unique<base::RunLoop>();
}

void NativeMessagingReaderTest::OnMessage(base::Value message) {
  message_ = std::move(message);
  run_loop_->Quit();
}

void NativeMessagingReaderTest::OnError() {
  on_error_signaled_ = true;
  run_loop_->Quit();
}

void NativeMessagingReaderTest::WriteMessage(const std::string& message) {
  uint32_t length = message.length();
  WriteData(reinterpret_cast<char*>(&length), 4);
  WriteData(message.data(), length);
}

void NativeMessagingReaderTest::WriteData(const char* data, int length) {
  int written = write_file_.WriteAtCurrentPos(data, length);
  ASSERT_EQ(length, written);
}

TEST_F(NativeMessagingReaderTest, ReaderDestroyedByClosingPipe) {
  WriteMessage("{\"foo\": 42}");
  RunAndWaitForOperationComplete();
  ASSERT_FALSE(on_error_signaled_);

  // Close the write end of the pipe while the reader is waiting for more data.
  write_file_.Close();
  RunAndWaitForOperationComplete();
  ASSERT_TRUE(on_error_signaled_);
}

#if BUILDFLAG(IS_WIN)
// This scenario is only a problem on Windows as closing the write pipe there
// does not trigger the parent process to close the read pipe.
// TODO(crbug.com/40221037) Disabled because it's flaky.
TEST_F(NativeMessagingReaderTest, DISABLED_ReaderDestroyedByOwner) {
  WriteMessage("{\"foo\": 42}");
  RunAndWaitForOperationComplete();
  ASSERT_FALSE(on_error_signaled_);

  // Destroy the reader while it is waiting for more data.
  reader_.reset();
  ASSERT_FALSE(on_error_signaled_);
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(NativeMessagingReaderTest, SingleGoodMessage) {
  WriteMessage("{\"foo\": 42}");
  RunAndWaitForOperationComplete();
  ASSERT_FALSE(on_error_signaled_);
  ASSERT_TRUE(message_);

  ASSERT_TRUE(message_->is_dict());
  std::optional<int> result = message_->GetDict().FindInt("foo");
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(42, result);
}

TEST_F(NativeMessagingReaderTest, MultipleGoodMessages) {
  {
    WriteMessage("{}");
    RunAndWaitForOperationComplete();
    ASSERT_FALSE(on_error_signaled_);
    ASSERT_TRUE(message_);
    ASSERT_TRUE(message_->is_dict());
    ASSERT_TRUE(message_->GetDict().empty());
  }

  {
    WriteMessage("{\"foo\": 42}");
    RunAndWaitForOperationComplete();
    ASSERT_FALSE(on_error_signaled_);
    ASSERT_TRUE(message_);
    ASSERT_TRUE(message_->is_dict());
    std::optional<int> result = message_->GetDict().FindInt("foo");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(42, result);
  }

  {
    WriteMessage("{\"bar\": 43}");
    RunAndWaitForOperationComplete();
    ASSERT_FALSE(on_error_signaled_);
    ASSERT_TRUE(message_);
    ASSERT_TRUE(message_->is_dict());
    std::optional<int> result = message_->GetDict().FindInt("bar");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(43, result);
  }

  {
    WriteMessage("{\"baz\": 44}");
    RunAndWaitForOperationComplete();
    ASSERT_FALSE(on_error_signaled_);
    ASSERT_TRUE(message_);
    ASSERT_TRUE(message_->is_dict());
    std::optional<int> result = message_->GetDict().FindInt("baz");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(44, result);
  }
}

TEST_F(NativeMessagingReaderTest, InvalidLength) {
  uint32_t length = 0xffffffff;
  WriteData(reinterpret_cast<char*>(&length), 4);
  RunAndWaitForOperationComplete();
  ASSERT_FALSE(message_);
  ASSERT_TRUE(on_error_signaled_);
}

TEST_F(NativeMessagingReaderTest, EmptyFile) {
  write_file_.Close();
  RunAndWaitForOperationComplete();
  ASSERT_FALSE(message_);
  ASSERT_TRUE(on_error_signaled_);
}

TEST_F(NativeMessagingReaderTest, ShortHeader) {
  // Write only 3 bytes - the message length header is supposed to be 4 bytes.
  WriteData("xxx", 3);
  write_file_.Close();
  RunAndWaitForOperationComplete();
  ASSERT_FALSE(message_);
  ASSERT_TRUE(on_error_signaled_);
}

TEST_F(NativeMessagingReaderTest, EmptyBody) {
  uint32_t length = 1;
  WriteData(reinterpret_cast<char*>(&length), 4);
  write_file_.Close();
  RunAndWaitForOperationComplete();
  ASSERT_FALSE(message_);
  ASSERT_TRUE(on_error_signaled_);
}

TEST_F(NativeMessagingReaderTest, ShortBody) {
  uint32_t length = 2;
  WriteData(reinterpret_cast<char*>(&length), 4);

  // Only write 1 byte, where the header indicates there should be 2 bytes.
  WriteData("x", 1);
  write_file_.Close();
  RunAndWaitForOperationComplete();
  ASSERT_FALSE(message_);
  ASSERT_TRUE(on_error_signaled_);
}

TEST_F(NativeMessagingReaderTest, InvalidJSON) {
  std::string text = "{";
  WriteMessage(text);
  write_file_.Close();
  RunAndWaitForOperationComplete();
  ASSERT_FALSE(message_);
  ASSERT_TRUE(on_error_signaled_);
}

}  // namespace remoting
