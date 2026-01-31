// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/win_utils.h"

#include <windows.h>

#include <ntstatus.h>
#include <psapi.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/tests/common/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

class ScopedTerminateProcess {
 public:
  explicit ScopedTerminateProcess(HANDLE process) : process_(process) {}

  ~ScopedTerminateProcess() { ::TerminateProcess(process_, 0); }

 private:
  HANDLE process_;
};

std::wstring GetRandomName() {
  return base::ASCIIToWide(
      base::StringPrintf("chrome_%016" PRIX64 "%016" PRIX64, base::RandUint64(),
                         base::RandUint64()));
}

void CompareHandlePath(const base::win::ScopedHandle& handle,
                       const std::wstring& expected_path) {
  auto path = GetPathFromHandle(handle.get());
  ASSERT_TRUE(path.has_value());
  EXPECT_TRUE(base::EqualsCaseInsensitiveASCII(path.value(), expected_path));
}

void CompareHandleType(const base::win::ScopedHandle& handle,
                       const std::wstring& expected_type) {
  auto type_name = GetTypeNameFromHandle(handle.get());
  ASSERT_TRUE(type_name);
  EXPECT_TRUE(
      base::EqualsCaseInsensitiveASCII(type_name.value(), expected_type));
}

void ExpectEnvironmentBlock(const std::vector<std::wstring>& vars,
                            const std::wstring& block) {
  std::wstring expected;
  for (const auto& var : vars) {
    expected += var;
    expected.push_back('\0');
  }
  expected.push_back('\0');
  EXPECT_EQ(expected, block);
}

}  // namespace

TEST(WinUtils, IsPipe) {
  using sandbox::IsPipe;

  std::wstring pipe_name = L"\\??\\pipe\\mypipe";
  EXPECT_TRUE(IsPipe(pipe_name));

  pipe_name = L"\\??\\PiPe\\mypipe";
  EXPECT_TRUE(IsPipe(pipe_name));

  pipe_name = L"\\??\\pipe";
  EXPECT_FALSE(IsPipe(pipe_name));

  pipe_name = L"\\??\\_pipe_\\mypipe";
  EXPECT_FALSE(IsPipe(pipe_name));

  pipe_name = L"\\??\\ABCD\\mypipe";
  EXPECT_FALSE(IsPipe(pipe_name));

  // Written as two strings to prevent trigraph '?' '?' '/'.
  pipe_name =
      L"/?"
      L"?/pipe/mypipe";
  EXPECT_FALSE(IsPipe(pipe_name));

  pipe_name = L"\\XX\\pipe\\mypipe";
  EXPECT_FALSE(IsPipe(pipe_name));

  pipe_name = L"\\Device\\NamedPipe\\mypipe";
  EXPECT_FALSE(IsPipe(pipe_name));
}

TEST(WinUtils, NtStatusToWin32Error) {
  using sandbox::GetLastErrorFromNtStatus;
  EXPECT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            GetLastErrorFromNtStatus(STATUS_SUCCESS));
  EXPECT_EQ(static_cast<DWORD>(ERROR_NOT_SUPPORTED),
            GetLastErrorFromNtStatus(STATUS_NOT_SUPPORTED));
  EXPECT_EQ(static_cast<DWORD>(ERROR_ALREADY_EXISTS),
            GetLastErrorFromNtStatus(STATUS_OBJECT_NAME_COLLISION));
  EXPECT_EQ(static_cast<DWORD>(ERROR_ACCESS_DENIED),
            GetLastErrorFromNtStatus(STATUS_ACCESS_DENIED));
}

TEST(WinUtils, GetPathAndTypeFromHandle) {
  EXPECT_FALSE(GetPathFromHandle(nullptr));
  EXPECT_FALSE(GetTypeNameFromHandle(nullptr));
  std::wstring random_name = GetRandomName();
  ASSERT_FALSE(random_name.empty());
  std::wstring event_name = L"Global\\" + random_name;
  base::win::ScopedHandle event_handle(
      ::CreateEvent(nullptr, FALSE, FALSE, event_name.c_str()));
  ASSERT_TRUE(event_handle.is_valid());
  CompareHandlePath(event_handle, L"\\BaseNamedObjects\\" + random_name);
  CompareHandleType(event_handle, L"Event");
  std::wstring pipe_name = L"\\\\.\\pipe\\" + random_name;
  base::win::ScopedHandle pipe_handle(::CreateNamedPipe(
      pipe_name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE,
      PIPE_UNLIMITED_INSTANCES, 0, 0, NMPWAIT_USE_DEFAULT_WAIT, nullptr));
  ASSERT_TRUE(pipe_handle.is_valid());
  CompareHandlePath(pipe_handle, L"\\Device\\NamedPipe\\" + random_name);
  CompareHandleType(pipe_handle, L"File");
}

TEST(WinUtils, ContainsNulCharacter) {
  std::wstring str = L"ABC";
  EXPECT_FALSE(ContainsNulCharacter(str));
  str.push_back('\0');
  EXPECT_TRUE(ContainsNulCharacter(str));
  str += L"XYZ";
  EXPECT_TRUE(ContainsNulCharacter(str));
}

TEST(WinUtils, FilterEnvironment) {
  const wchar_t empty[] = L"";
  const wchar_t a2b3c4[] = L"A=2\0B=3\0C=4\0";
  const wchar_t xy1z[] = L"X\0Y=1\0Z\0";

  auto res = FilterEnvironment(empty, {});
  ExpectEnvironmentBlock({}, res);

  res = FilterEnvironment(a2b3c4, {});
  ExpectEnvironmentBlock({}, res);

  res = FilterEnvironment(a2b3c4, {L"B"});
  ExpectEnvironmentBlock({L"B=3"}, res);

  res = FilterEnvironment(empty, {L"B"});
  ExpectEnvironmentBlock({}, res);

  // D should be ignored, but B should still appear.
  res = FilterEnvironment(a2b3c4, {L"B", L"D"});
  ExpectEnvironmentBlock({L"B=3"}, res);

  // D should be ignored.
  res = FilterEnvironment(a2b3c4, {L"D"});
  ExpectEnvironmentBlock({}, res);

  // Once again D should be ignored but this time A and C should match.
  res = FilterEnvironment(a2b3c4, {L"D", L"A", L"C"});
  ExpectEnvironmentBlock({L"A=2", L"C=4"}, res);

  // Check that the parser works if the '=' character is missing.
  res = FilterEnvironment(xy1z, {L"X", L"Z"});
  ExpectEnvironmentBlock({L"X", L"Z"}, res);
}

}  // namespace sandbox
