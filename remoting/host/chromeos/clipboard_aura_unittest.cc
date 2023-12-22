// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/clipboard_aura.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "remoting/base/constants.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

using testing::_;
using testing::Eq;
using testing::InvokeWithoutArgs;
using testing::Property;

namespace remoting {

namespace {

const base::TimeDelta kTestOverridePollingInterval = base::Milliseconds(1);

class ClientClipboard : public protocol::ClipboardStub {
 public:
  ClientClipboard();

  ClientClipboard(const ClientClipboard&) = delete;
  ClientClipboard& operator=(const ClientClipboard&) = delete;

  MOCK_METHOD1(InjectClipboardEvent,
               void(const protocol::ClipboardEvent& event));
};

ClientClipboard::ClientClipboard() = default;

}  // namespace

class ClipboardAuraTest : public testing::Test {
 public:
  ClipboardAuraTest() = default;
  void SetUp() override;
  void TearDown() override;

 protected:
  void StopAndResetClipboard();

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  raw_ptr<ClientClipboard, DanglingUntriaged> client_clipboard_;
  std::unique_ptr<ClipboardAura> clipboard_;
};

void ClipboardAuraTest::SetUp() {
  // Alert the clipboard class to which threads are allowed to access the
  // clipboard.
  std::vector<base::PlatformThreadId> allowed_clipboard_threads;
  allowed_clipboard_threads.push_back(base::PlatformThread::CurrentId());
  ui::Clipboard::SetAllowedThreads(allowed_clipboard_threads);

  // Setup the clipboard.
  client_clipboard_ = new ClientClipboard();
  clipboard_ = std::make_unique<ClipboardAura>();

  EXPECT_GT(TestTimeouts::tiny_timeout(), kTestOverridePollingInterval * 10)
      << "The test timeout should be greater than the polling interval";
  clipboard_->SetPollingIntervalForTesting(kTestOverridePollingInterval);

  clipboard_->Start(base::WrapUnique(client_clipboard_.get()));
}

void ClipboardAuraTest::TearDown() {
  ui::Clipboard::DestroyClipboardForCurrentThread();
}

void ClipboardAuraTest::StopAndResetClipboard() {
  clipboard_.reset();
}

TEST_F(ClipboardAuraTest, WriteToClipboard) {
  protocol::ClipboardEvent event;
  event.set_mime_type(kMimeTypeTextUtf8);
  event.set_data("Test data.");

  clipboard_->InjectClipboardEvent(event);
  StopAndResetClipboard();
  base::RunLoop().RunUntilIdle();

  std::string clipboard_data;
  ui::Clipboard* aura_clipboard = ui::Clipboard::GetForCurrentThread();
  aura_clipboard->ReadAsciiText(ui::ClipboardBuffer::kCopyPaste,
                                /* data_dst = */ nullptr, &clipboard_data);

  EXPECT_EQ(clipboard_data, "Test data.")
      << "InjectClipboardEvent should write to aura clipboard";
}

TEST_F(ClipboardAuraTest, MonitorClipboardChanges) {
  base::RunLoop().RunUntilIdle();

  {
    // |clipboard_writer| will write to the clipboard when it goes out of scope.
    ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(u"Test data.");
  }

  EXPECT_CALL(*client_clipboard_,
              InjectClipboardEvent(
                  Property(&protocol::ClipboardEvent::data, Eq("Test data."))))
      .Times(1);

  base::RunLoop run_loop;
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ClipboardAuraTest_MonitorClipboardChanges_Test::
                         StopAndResetClipboard,
                     base::Unretained(this)),
      TestTimeouts::tiny_timeout());
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
}

}  // namespace remoting
