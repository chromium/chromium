// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/platform_handle_security_util_win.h"

#include <windows.h>

#include <optional>

#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/win/nt_status.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_util.h"

namespace mojo {

namespace {

FileHandleSecurityErrorCallback& GetErrorCallback() {
  static base::NoDestructor<FileHandleSecurityErrorCallback> callback;
  return *callback;
}

#if DCHECK_IS_ON()

std::wstring GetPathFromHandle(HANDLE handle) {
  std::wstring full_path(MAX_PATH - 1, '\0');
  // Note: the math here is a bit messy. `basic_string` guarantees that enough
  // space is reserved so that index may be any value between 0 and size()
  // inclusive. However, `GetFinalPathNameByHandleW()` and `MAX_PATH` include
  // the NUL terminator as part of the size (e.g. MAX_PATH is 3 characters for
  // the drive letter, 256 characters for the path, and 1 character for NUL),
  // hence `- 1` for the `resize()` calls.
  DWORD result =
      ::GetFinalPathNameByHandleW(handle, full_path.data(), MAX_PATH, 0);
  if (result > MAX_PATH) {
    full_path.resize(result - 1);
    result = ::GetFinalPathNameByHandleW(handle, full_path.data(), result, 0);
  }
  if (!result) {
    PLOG(ERROR) << "Could not get full path for handle " << handle;
    return std::wstring();
  }
  full_path.resize(result);
  return full_path;
}

std::optional<bool> IsReadOnlyHandle(HANDLE handle) {
  std::optional<ACCESS_MASK> flags = base::win::GetGrantedAccess(handle);
  if (!flags.has_value()) {
    return std::nullopt;
  }
  // Cannot use GENERIC_WRITE as that includes SYNCHRONIZE.
  // This is ~(all the writable permissions).
  return !(flags.value() &
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

  std::optional<bool> is_read_only = IsReadOnlyHandle(handle);
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

  // If the file cannot be opened with FILE_EXECUTE it means that it is safe.
  if (!file_handle.is_valid()) {
    return;
  }

  auto& error_callback = GetErrorCallback();
  if (error_callback) {
    bool handled = error_callback.Run();
    if (handled) {
      return;
    }
  }

  DLOG(FATAL) << "Transfer of writable handle to executable file to an "
                 "untrusted process: "
              << path
              << ". Please use AddFlagsForPassingToUntrustedProcess or call "
                 "PreventExecuteMapping on the FilePath.";
#endif  // DCHECK_IS_ON();
}

void SetUnsafeFileHandleCallbackForTesting(
    FileHandleSecurityErrorCallback callback) {
  GetErrorCallback() = std::move(callback);
}

}  // namespace mojo
