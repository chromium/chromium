// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/base/process_util.h"

#include <windows.h>

#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/win/scoped_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

class ScopedStdHandle {
 public:
  ScopedStdHandle(DWORD std_handle, HANDLE new_handle)
      : std_handle_(std_handle), old_handle_(::GetStdHandle(std_handle)) {
    ::SetStdHandle(std_handle_, new_handle);
  }

  ~ScopedStdHandle() { ::SetStdHandle(std_handle_, old_handle_); }

 private:
  DWORD std_handle_;
  HANDLE old_handle_;
};

}  // namespace

TEST(ProcessUtilWinTest, NonPipeStdinReturnsNull) {
  base::win::ScopedHandle event(::CreateEvent(nullptr, TRUE, FALSE, nullptr));
  ASSERT_TRUE(event.is_valid());
  ASSERT_EQ(GetLauncherProcessIdFromPipes(event.get(), event.get()),
            base::kNullProcessId);

  ScopedStdHandle scoped_stdin(STD_INPUT_HANDLE, event.get());
  ASSERT_EQ(GetLauncherProcessIdFromStdioPipes(), base::kNullProcessId);
}

TEST(ProcessUtilWinTest, PipeServerPidResolution) {
  HANDLE read_pipe = nullptr;
  HANDLE write_pipe = nullptr;
  SECURITY_ATTRIBUTES sa = {};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  ASSERT_TRUE(::CreatePipe(&read_pipe, &write_pipe, &sa, 0));

  base::win::ScopedHandle scoped_read(read_pipe);
  base::win::ScopedHandle scoped_write(write_pipe);

  ASSERT_EQ(GetLauncherProcessIdFromPipes(read_pipe, write_pipe),
            base::GetCurrentProcId());

  ScopedStdHandle scoped_stdin(STD_INPUT_HANDLE, read_pipe);
  ScopedStdHandle scoped_stdout(STD_OUTPUT_HANDLE, write_pipe);
  ASSERT_EQ(GetLauncherProcessIdFromStdioPipes(), base::GetCurrentProcId());
}

TEST(ProcessUtilWinTest, NullOrInvalidHandlesReturnNull) {
  HANDLE read_pipe = nullptr;
  HANDLE write_pipe = nullptr;
  SECURITY_ATTRIBUTES sa = {};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  ASSERT_TRUE(::CreatePipe(&read_pipe, &write_pipe, &sa, 0));

  base::win::ScopedHandle scoped_read(read_pipe);
  base::win::ScopedHandle scoped_write(write_pipe);

  ASSERT_EQ(GetLauncherProcessIdFromPipes(nullptr, write_pipe),
            base::kNullProcessId);
  ASSERT_EQ(GetLauncherProcessIdFromPipes(read_pipe, nullptr),
            base::kNullProcessId);
  ASSERT_EQ(GetLauncherProcessIdFromPipes(INVALID_HANDLE_VALUE, write_pipe),
            base::kNullProcessId);
  ASSERT_EQ(GetLauncherProcessIdFromPipes(read_pipe, INVALID_HANDLE_VALUE),
            base::kNullProcessId);
}

TEST(ProcessUtilWinTest, MismatchedOrNonPipeHandlesReturnNull) {
  HANDLE read_pipe = nullptr;
  HANDLE write_pipe = nullptr;
  SECURITY_ATTRIBUTES sa = {};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  ASSERT_TRUE(::CreatePipe(&read_pipe, &write_pipe, &sa, 0));

  base::win::ScopedHandle scoped_read(read_pipe);
  base::win::ScopedHandle scoped_write(write_pipe);

  base::win::ScopedHandle event(::CreateEvent(nullptr, TRUE, FALSE, nullptr));
  ASSERT_TRUE(event.is_valid());

  ASSERT_EQ(GetLauncherProcessIdFromPipes(read_pipe, event.get()),
            base::kNullProcessId);
  ASSERT_EQ(GetLauncherProcessIdFromPipes(event.get(), write_pipe),
            base::kNullProcessId);
}

}  // namespace remoting
