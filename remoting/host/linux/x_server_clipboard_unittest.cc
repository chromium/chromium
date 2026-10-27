// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/x_server_clipboard.h"

#include <sys/file.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "remoting/base/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/x/connection.h"

namespace remoting {

namespace {

class ClipboardTestClient : public x11::EventObserver {
 public:
  ClipboardTestClient() = default;

  ClipboardTestClient(const ClipboardTestClient&) = delete;
  ClipboardTestClient& operator=(const ClipboardTestClient&) = delete;

  ~ClipboardTestClient() override {
    if (connection_) {
      connection_->RemoveEventObserver(this);
    }
  }

  void Init(x11::Connection* connection) {
    connection_ = connection;
    connection_->AddEventObserver(this);
    clipboard_.Init(connection, base::BindRepeating(
                                    &ClipboardTestClient::OnClipboardChanged,
                                    base::Unretained(this)));
  }

  void SetClipboardData(const std::string& clipboard_data) {
    clipboard_data_ = clipboard_data;
    clipboard_.SetClipboard(kMimeTypeTextUtf8, clipboard_data);
  }

  void OnClipboardChanged(const std::string& mime_type,
                          const std::string& data) {
    clipboard_data_ = data;
  }

  // Process X events on the connection, returning true if any events were
  // processed.
  bool PumpXEvents() {
    dispatched_event_ = false;
    connection_->Sync();
    connection_->DispatchAll();
    return dispatched_event_;
  }

  void OnEvent(const x11::Event& event) override {
    dispatched_event_ = true;
    clipboard_.ProcessXEvent(event);
  }

  const std::string& clipboard_data() const { return clipboard_data_; }
  x11::Connection* connection() const { return connection_; }

 private:
  std::string clipboard_data_;
  XServerClipboard clipboard_;
  raw_ptr<x11::Connection> connection_ = nullptr;
  bool dispatched_event_ = false;
};

}  // namespace

class XServerClipboardTest : public testing::Test {
 public:
  void SetUp() override {
    // Serialize access to the shared X11 clipboard across parallel test
    // processes on the same X display (e.g. when run with
    // --test-launcher-bot-mode and --gtest_repeat).
    base::FilePath temp_dir;
    ASSERT_TRUE(base::GetTempDir(&temp_dir));
    std::string display =
        base::Environment::Create()->GetVar("DISPLAY").value_or("default");
    base::ReplaceChars(display, "/", "_", &display);
    std::string lock_filename =
        base::StringPrintf("remoting_x_server_clipboard_unittest_%d_%s.lock",
                           geteuid(), display.c_str());
    lock_file_.Initialize(
        temp_dir.AppendASCII(lock_filename),
        base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
    ASSERT_TRUE(lock_file_.IsValid());
    ASSERT_EQ(HANDLE_EINTR(flock(lock_file_.GetPlatformFile(), LOCK_EX)), 0);

    // SynchronizeForTest() ensures that PumpXEvents() fully processes all X
    // server requests and responses before returning to the caller.
    connection1_ = std::make_unique<x11::Connection>();
    connection1_->SynchronizeForTest(true);
    client1_.Init(connection1_.get());
    connection2_ = std::make_unique<x11::Connection>();
    connection2_->SynchronizeForTest(true);
    client2_.Init(connection2_.get());
  }

  void PumpXEvents() {
    while (true) {
      if (!client1_.PumpXEvents() && !client2_.PumpXEvents()) {
        break;
      }
    }
  }

  // `lock_file_` must outlive the X11 connections, and the connections must
  // outlive `client1_` and `client2_`.
  base::File lock_file_;
  std::unique_ptr<x11::Connection> connection1_;
  std::unique_ptr<x11::Connection> connection2_;
  ClipboardTestClient client1_;
  ClipboardTestClient client2_;
};

TEST_F(XServerClipboardTest, CopyPaste) {
  // Verify clipboard data can be transferred more than once. Then verify that
  // the code continues to function in the opposite direction (so client1_ will
  // send then receive, and client2_ will receive then send).
  client1_.SetClipboardData("Text1");
  PumpXEvents();
  EXPECT_EQ(client2_.clipboard_data(), "Text1");

  client1_.SetClipboardData("Text2");
  PumpXEvents();
  EXPECT_EQ(client2_.clipboard_data(), "Text2");

  client2_.SetClipboardData("Text3");
  PumpXEvents();
  EXPECT_EQ(client1_.clipboard_data(), "Text3");

  client2_.SetClipboardData("Text4");
  PumpXEvents();
  EXPECT_EQ(client1_.clipboard_data(), "Text4");
}

}  // namespace remoting
