// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/clipboard_win.h"

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "remoting/base/constants.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Return;

class MockWin32Clipboard : public Win32Clipboard {
 public:
  MockWin32Clipboard() = default;
  ~MockWin32Clipboard() override = default;

  MOCK_METHOD(bool, Open, (HWND hwnd), (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(bool, Empty, (), (override));
  MOCK_METHOD(bool, SetData, (UINT format, HGLOBAL data), (override));
  MOCK_METHOD(HGLOBAL, GetData, (UINT format), (override));
  MOCK_METHOD(bool, IsFormatAvailable, (UINT format), (override));
  MOCK_METHOD(bool, AddFormatListener, (HWND hwnd), (override));
  MOCK_METHOD(bool, RemoveFormatListener, (HWND hwnd), (override));
};

}  // namespace

class ClipboardWinTest : public testing::Test {
 public:
  ClipboardWinTest() {
    auto mock_api = std::make_unique<MockWin32Clipboard>();
    mock_api_ = mock_api.get();
    clipboard_ = std::make_unique<ClipboardWin>(std::move(mock_api));
  }

  void SetUp() override {
    auto stub = std::make_unique<protocol::MockClipboardStub>();
    mock_stub_ = stub.get();
    EXPECT_CALL(*mock_api_, AddFormatListener(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_api_, RemoveFormatListener(_))
        .WillRepeatedly(Return(true));
    clipboard_->Start(std::move(stub));
  }

  void TriggerOnClipboardUpdate() { clipboard_->OnClipboardUpdate(); }

  ~ClipboardWinTest() override {
    mock_stub_ = nullptr;
    mock_api_ = nullptr;
    clipboard_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  raw_ptr<MockWin32Clipboard> mock_api_;
  raw_ptr<protocol::MockClipboardStub> mock_stub_;
  std::unique_ptr<ClipboardWin> clipboard_;
};

TEST_F(ClipboardWinTest, Create) {
  ASSERT_TRUE(clipboard_);
}

TEST_F(ClipboardWinTest, InjectClipboardEvent) {
  protocol::ClipboardEvent event;
  event.set_mime_type(kMimeTypeTextUtf8);
  event.set_data("test");

  EXPECT_CALL(*mock_api_, Open(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_api_, Empty()).WillOnce(Return(true));
  EXPECT_CALL(*mock_api_, SetData(CF_UNICODETEXT, _))
      .WillOnce([](UINT format, HGLOBAL data) {
        ::GlobalFree(data);
        return true;
      });
  EXPECT_CALL(*mock_api_, Close()).Times(1);

  clipboard_->InjectClipboardEvent(event);
}

TEST_F(ClipboardWinTest, InjectClipboardEvent_LargePayload) {
  protocol::ClipboardEvent event;
  event.set_mime_type(kMimeTypeTextUtf8);
  // 1MB + 1 byte.
  std::string large_data(1024 * 1024 + 1, 'A');
  event.set_data(large_data);

  // Should be dropped.
  EXPECT_CALL(*mock_api_, Open(_)).Times(0);

  clipboard_->InjectClipboardEvent(event);
}

TEST_F(ClipboardWinTest, OnClipboardUpdate) {
  EXPECT_CALL(*mock_api_, IsFormatAvailable(CF_UNICODETEXT))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_api_, Open(_)).WillOnce(Return(true));

  const wchar_t kData[] = L"monitor_test";
  HGLOBAL h_mem = ::GlobalAlloc(GMEM_MOVEABLE, sizeof(kData));
  void* ptr = ::GlobalLock(h_mem);
  // SAFETY: ptr points to at least sizeof(kData) bytes.
  auto ptr_span =
      UNSAFE_BUFFERS(base::span(static_cast<wchar_t*>(ptr), std::size(kData)));
  base::as_writable_bytes(ptr_span).copy_from(
      base::as_bytes(base::span(kData)));
  ::GlobalUnlock(h_mem);

  EXPECT_CALL(*mock_api_, GetData(CF_UNICODETEXT)).WillOnce(Return(h_mem));
  EXPECT_CALL(*mock_api_, Close()).Times(1);

  EXPECT_CALL(*mock_stub_, InjectClipboardEvent(_))
      .WillOnce([&](const protocol::ClipboardEvent& event) {
        EXPECT_EQ(event.mime_type(), kMimeTypeTextUtf8);
        EXPECT_EQ(event.data(), "monitor_test");
      });

  TriggerOnClipboardUpdate();

  // GetData returns memory owned by the clipboard (system), but since we are
  // mocking it, we must free it in the test.
  ::GlobalFree(h_mem);
}

TEST_F(ClipboardWinTest, OnClipboardUpdate_SafeRead) {
  EXPECT_CALL(*mock_api_, IsFormatAvailable(CF_UNICODETEXT))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_api_, Open(_)).WillOnce(Return(true));

  // Data WITHOUT null terminator.
  const wchar_t kData[] = {L'A', L'B', L'C', L'D'};
  // We use GMEM_ZEROINIT to ensure that any padding added by the system for
  // heap alignment is deterministic (zeros). This prevents flakiness from
  // reading uninitialized memory if the buffer is rounded up by GlobalAlloc.
  HGLOBAL h_mem = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(kData));
  void* ptr = ::GlobalLock(h_mem);
  // SAFETY: ptr points to at least sizeof(kData) bytes.
  auto ptr_span =
      UNSAFE_BUFFERS(base::span(static_cast<wchar_t*>(ptr), std::size(kData)));
  base::as_writable_bytes(ptr_span).copy_from(
      base::as_bytes(base::span(kData)));
  ::GlobalUnlock(h_mem);

  EXPECT_CALL(*mock_api_, GetData(CF_UNICODETEXT)).WillOnce(Return(h_mem));
  EXPECT_CALL(*mock_api_, Close()).Times(1);

  EXPECT_CALL(*mock_stub_, InjectClipboardEvent(_))
      .WillOnce([&](const protocol::ClipboardEvent& event) {
        EXPECT_EQ(event.data(), "ABCD");
      });

  TriggerOnClipboardUpdate();

  ::GlobalFree(h_mem);
}

TEST_F(ClipboardWinTest, OnClipboardUpdate_LargePayload) {
  EXPECT_CALL(*mock_api_, IsFormatAvailable(CF_UNICODETEXT))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_api_, Open(_)).WillOnce(Return(true));

  // 1MB + 1 character (UTF-16).
  const size_t kMaxClipboardSize = 1024 * 1024;
  std::vector<wchar_t> large_data(kMaxClipboardSize + 1, L'A');
  HGLOBAL h_mem =
      ::GlobalAlloc(GMEM_MOVEABLE, large_data.size() * sizeof(wchar_t));
  void* ptr = ::GlobalLock(h_mem);
  // SAFETY: ptr points to at least large_data.size() * sizeof(wchar_t) bytes.
  auto ptr_span =
      UNSAFE_BUFFERS(base::span(static_cast<wchar_t*>(ptr), large_data.size()));
  base::as_writable_bytes(ptr_span).copy_from(
      base::as_bytes(base::span(large_data)));
  ::GlobalUnlock(h_mem);

  EXPECT_CALL(*mock_api_, GetData(CF_UNICODETEXT)).WillOnce(Return(h_mem));
  EXPECT_CALL(*mock_api_, Close()).Times(1);

  // Should be dropped.
  EXPECT_CALL(*mock_stub_, InjectClipboardEvent(_)).Times(0);

  TriggerOnClipboardUpdate();

  ::GlobalFree(h_mem);
}

TEST_F(ClipboardWinTest, InjectClipboardEvent_SetDataFailure) {
  protocol::ClipboardEvent event;
  event.set_mime_type(kMimeTypeTextUtf8);
  event.set_data("test");

  EXPECT_CALL(*mock_api_, Open(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_api_, Empty()).WillOnce(Return(true));
  EXPECT_CALL(*mock_api_, SetData(CF_UNICODETEXT, _)).WillOnce(Return(false));
  EXPECT_CALL(*mock_api_, Close()).Times(1);

  // Should handle failure without crash.
  clipboard_->InjectClipboardEvent(event);
}

TEST_F(ClipboardWinTest, InjectClipboardEvent_NoWindow) {
  // Create a new clipboard without starting it (so window_ is null).
  auto mock_api = std::make_unique<MockWin32Clipboard>();
  ClipboardWin clipboard(std::move(mock_api));

  protocol::ClipboardEvent event;
  event.set_mime_type(kMimeTypeTextUtf8);
  event.set_data("test");

  // Should return early without crash.
  clipboard.InjectClipboardEvent(event);
}

}  // namespace remoting
