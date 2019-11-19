// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/native_messaging/native_messaging_reader.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
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
  void OnMessage(std::unique_ptr<base::Value> message);

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
  std::unique_ptr<base::Value> message_;

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
  reader_.reset(new NativeMessagingReader(std::move(read_file_)));
  run_loop_.reset(new base::RunLoop());

  // base::Unretained is safe since no further tasks can run after
  // RunLoop::Run() returns.
  reader_->Start(
      base::Bind(&NativeMessagingReaderTest::OnMessage, base::Unretained(this)),
      base::Bind(&NativeMessagingReaderTest::OnError, base::Unretained(this)));
}

void NativeMessagingReaderTest::RunAndWaitForOperationComplete() {
  run_loop_->Run();
  run_loop_.reset(new base::RunLoop());
}

void NativeMessagingReaderTest::OnMessage(
    std::unique_ptr<base::Value> message) {
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

#if defined(OS_WIN)
// This scenario is only a problem on Windows as closing the write pipe there
// does not trigger the parent process to close the read pipe.
TEST_F(NativeMessagingReaderTest, ReaderDestroyedByOwner) {
  WriteMessage("{\"foo\": 42}");
  RunAndWaitForOperationComplete();
  ASSERT_FALSE(on_error_signaled_);

  // Destroy the reader while it is waiting for more data.
  reader_.reset();
  ASSERT_FALSE(on_error_signaled_);
}
#endif  // defined(OS_WIN)

TEST_F(NativeMessagingReaderTest, SingleGoodMessage) {
  WriteMessage("{\"foo\": 42}");
  RunAndWaitForOperationComplete();
  ASSERT_FALSE(on_error_signaled_);
  ASSERT_TRUE(message_);
  base::DictionaryValue* message_dict;
  ASSERT_TRUE(message_->GetAsDictionary(&message_dict));
  int result;
  ASSERT_TRUE(message_dict->GetInteger("foo", &result));
  ASSERT_EQ(42, result);
}

TEST_F(NativeMessagingReaderTest, MultipleGoodMessages) {
  WriteMessage("{}");
  RunAndWaitForOperationComplete();
  ASSERT_FALSE(on_error_signaled_);
  ASSERT_TRUE(message_);
  base::DictionaryValue* message_dict;
  ASSERT_TRUE(message_->GetAsDictionary(&message_dict));

  int result;
  WriteMessage("{\"foo\": 42}");
  RunAndWaitForOperationComplete();
  ASSERT_FALSE(on_error_signaled_);
  ASSERT_TRUE(message_);
  ASSERT_TRUE(message_->GetAsDictionary(&message_dict));
  ASSERT_TRUE(message_dict->GetInteger("foo", &result));
  ASSERT_EQ(42, result);

  WriteMessage("{\"bar\": 43}");
  RunAndWaitForOperationComplete();
  ASSERT_FALSE(on_error_signaled_);
  ASSERT_TRUE(message_);
  ASSERT_TRUE(message_->GetAsDictionary(&message_dict));
  ASSERT_TRUE(message_dict->GetInteger("bar", &result));
  ASSERT_EQ(43, result);

  WriteMessage("{\"baz\": 44}");
  RunAndWaitForOperationComplete();
  ASSERT_FALSE(on_error_signaled_);
  ASSERT_TRUE(message_);
  ASSERT_TRUE(message_->GetAsDictionary(&message_dict));
  ASSERT_TRUE(message_dict->GetInteger("baz", &result));
  ASSERT_EQ(44, result);
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
