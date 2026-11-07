// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/launch_process_with_token.h"

#include <windows.h>

#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/win/scoped_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

base::win::ScopedHandle DuplicateCurrentProcessPrimaryToken() {
  HANDLE process_token_raw = nullptr;
  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY,
                          &process_token_raw)) {
    return base::win::ScopedHandle();
  }
  base::win::ScopedHandle process_token(process_token_raw);

  HANDLE primary_token_raw = nullptr;
  if (!::DuplicateTokenEx(process_token.Get(), MAXIMUM_ALLOWED,
                          /*lpTokenAttributes=*/nullptr, SecurityAnonymous,
                          TokenPrimary, &primary_token_raw)) {
    return base::win::ScopedHandle();
  }
  return base::win::ScopedHandle(primary_token_raw);
}

}  // namespace

TEST(LaunchProcessWithTokenTest, CapturesLastErrorOnFailure) {
  base::win::ScopedHandle primary_token = DuplicateCurrentProcessPrimaryToken();
  ASSERT_TRUE(primary_token.is_valid());

  SECURITY_ATTRIBUTES inheritable_attributes = {
      .nLength = sizeof(SECURITY_ATTRIBUTES),
      .lpSecurityDescriptor = nullptr,
      .bInheritHandle = TRUE,
  };
  base::win::ScopedHandle inheritable_event(
      ::CreateEventW(&inheritable_attributes, /*bManualReset=*/TRUE,
                     /*bInitialState=*/FALSE, /*lpName=*/nullptr));
  ASSERT_TRUE(inheritable_event.is_valid());

  base::HandlesToInheritVector handles_to_inherit = {inheritable_event.Get()};
  base::win::ScopedHandle process_out;
  base::win::ScopedHandle thread_out;
  DWORD last_error = ERROR_SUCCESS;

  bool launched = LaunchProcessWithToken(
      base::FilePath(L"C:\\nonexistent_chromoting_test_binary.exe"),
      L"\"C:\\nonexistent_chromoting_test_binary.exe\"", primary_token.Get(),
      /*process_attributes=*/nullptr, /*thread_attributes=*/nullptr,
      handles_to_inherit, /*security_capabilities=*/nullptr,
      /*creation_flags=*/0, /*desktop_name=*/nullptr, &process_out, &thread_out,
      &last_error);

  EXPECT_FALSE(launched);
  EXPECT_EQ(last_error, static_cast<DWORD>(ERROR_FILE_NOT_FOUND));
}

}  // namespace remoting
