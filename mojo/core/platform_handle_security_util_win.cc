// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/platform_handle_security_util_win.h"

#include <windows.h>
#include <winternl.h>

#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/win/nt_status.h"
#include "base/win/scoped_handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo::core {

namespace {

#if DCHECK_IS_ON()

std::wstring GetPathFromHandle(HANDLE handle) {
  std::wstring full_path;
  DWORD result = ::GetFinalPathNameByHandleW(
      handle, base::WriteInto(&full_path, MAX_PATH + 1), MAX_PATH + 1, 0);
  if (result > MAX_PATH) {
    result = ::GetFinalPathNameByHandleW(
        handle, base::WriteInto(&full_path, result), result, 0);
  }
  if (!result) {
    PLOG(ERROR) << "Could not get full path for handle " << handle;
    return std::wstring();
  }
  full_path.resize(result);
  return full_path;
}

absl::optional<bool> IsReadOnlyHandle(HANDLE handle) {
  static const auto nt_query_object =
      reinterpret_cast<decltype(&NtQueryObject)>(
          GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQueryObject"));
  if (!nt_query_object) {
    return absl::nullopt;
  }

  PUBLIC_OBJECT_BASIC_INFORMATION basic_info = {};
  if (!NT_SUCCESS(nt_query_object(handle, ObjectBasicInformation, &basic_info,
                                  sizeof(basic_info), nullptr))) {
    // If unable to query the object
    return absl::nullopt;
  }

  // Cannot use GENERIC_WRITE as that includes SYNCHRONIZE.
  // This is ~(all the writable permissions).
  return !(basic_info.GrantedAccess &
           (FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_DATA |
            FILE_WRITE_EA | WRITE_DAC | WRITE_OWNER | DELETE));
}

#endif  // DCHECK_IS_ON();

}  // namespace

void DcheckIfFileHandleIsUnsafe(HANDLE handle) {
#if DCHECK_IS_ON()
  if (!base::FeatureList::IsEnabled(
          base::features::kEnforceNoExecutableFileHandles)) {
    return;
  }

  if (GetFileType(handle) != FILE_TYPE_DISK) {
    return;
  }

  absl::optional<bool> is_read_only = IsReadOnlyHandle(handle);
  if (!is_read_only.has_value()) {
    // If unable to obtain whether or not the handle is read-only, skip the rest
    // of the checks, since it's likely GetPathFromHandle below would fail
    // anyway.
    return;
  }

  if (*is_read_only) {
    // Handle is read-only so considered safe.
    return;
  }

  std::wstring path = GetPathFromHandle(handle);
  if (path.empty()) {
    return;
  }

  base::win::ScopedHandle file_handle(
      ::CreateFileW(path.c_str(), FILE_READ_DATA | FILE_EXECUTE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
  CHECK(!file_handle.is_valid())
      << "Transfer of writable handle to executable "
         "file to an untrusted process: "
      << path
      << ". Please use AddFlagsForPassingToUntrustedProcess or call "
         "PreventExecuteMapping on the FilePath.";
#endif  // DCHECK_IS_ON();
}

}  // namespace mojo::core
