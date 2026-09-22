// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/service_util.h"

#include <windows.h>

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::installer {

namespace {

std::wstring CreateUniqueEventName(const wchar_t* prefix) {
  return base::StrCat(
      {prefix, L"_",
       base::UTF8ToWide(base::UnguessableToken::Create().ToString())});
}

}  // namespace

TEST(ServiceUtilTest, IsHostSessionActive_EventDoesNotExist) {
  std::wstring event_name = CreateUniqueEventName(L"Local\\TestSessionActive");
  EXPECT_FALSE(IsHostSessionActive(event_name.c_str()));
}

TEST(ServiceUtilTest, IsHostSessionActive_SignaledAndUnsignaled) {
  std::wstring event_name = CreateUniqueEventName(L"Local\\TestSessionActive");
  base::win::ScopedHandle event(::CreateEventW(nullptr, /*bManualReset=*/TRUE,
                                               /*bInitialState=*/FALSE,
                                               event_name.c_str()));
  ASSERT_TRUE(event.is_valid());

  // Unsignaled initially -> host is idle.
  EXPECT_FALSE(IsHostSessionActive(event_name.c_str()));

  // Signaled -> host session is active. Calling twice verifies non-consuming
  // check on manual-reset event.
  ASSERT_TRUE(::SetEvent(event.Get()));
  EXPECT_TRUE(IsHostSessionActive(event_name.c_str()));
  EXPECT_TRUE(IsHostSessionActive(event_name.c_str()));

  // Reset -> host is idle again.
  ASSERT_TRUE(::ResetEvent(event.Get()));
  EXPECT_FALSE(IsHostSessionActive(event_name.c_str()));
}

TEST(ServiceUtilTest, SignalHostUpdatePending_EventDoesNotExist) {
  std::wstring event_name = CreateUniqueEventName(L"Local\\TestUpdatePending");
  EXPECT_FALSE(SignalHostUpdatePending(event_name.c_str()));
}

TEST(ServiceUtilTest, SignalHostUpdatePending_SignalsExistingEvent) {
  std::wstring event_name = CreateUniqueEventName(L"Local\\TestUpdatePending");
  base::win::ScopedHandle event(::CreateEventW(nullptr, /*bManualReset=*/TRUE,
                                               /*bInitialState=*/FALSE,
                                               event_name.c_str()));
  ASSERT_TRUE(event.is_valid());

  EXPECT_EQ(::WaitForSingleObject(event.Get(), 0),
            static_cast<DWORD>(WAIT_TIMEOUT));

  EXPECT_TRUE(SignalHostUpdatePending(event_name.c_str()));
  EXPECT_EQ(::WaitForSingleObject(event.Get(), 0),
            static_cast<DWORD>(WAIT_OBJECT_0));
}

}  // namespace remoting::installer
