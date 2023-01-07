// Copyright 2006-2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/tests/common/test_utils.h"

#include <windows.h>

#include <stddef.h>
#include <winioctl.h>

#include "base/numerics/safe_conversions.h"

namespace sandbox {

typedef struct _REPARSE_DATA_BUFFER {
  ULONG  ReparseTag;
  USHORT  ReparseDataLength;
  USHORT  Reserved;
  union {
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      ULONG Flags;
      WCHAR PathBuffer[1];
      } SymbolicLinkReparseBuffer;
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      WCHAR PathBuffer[1];
      } MountPointReparseBuffer;
    struct {
      UCHAR DataBuffer[1];
    } GenericReparseBuffer;
  };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

// Sets a reparse point. |source| will now point to |target|. Returns true if
// the call succeeds, false otherwise.
bool SetReparsePoint(HANDLE source, const wchar_t* target) {
  USHORT size_target = static_cast<USHORT>(wcslen(target)) * sizeof(target[0]);

  char buffer[2000] = {0};
  DWORD returned;

  REPARSE_DATA_BUFFER* data = reinterpret_cast<REPARSE_DATA_BUFFER*>(buffer);

  data->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
  memcpy(data->MountPointReparseBuffer.PathBuffer, target, size_target + 2);
  data->MountPointReparseBuffer.SubstituteNameLength = size_target;
  data->MountPointReparseBuffer.PrintNameOffset = size_target + 2;
  data->ReparseDataLength = size_target + 4 + 8;

  int data_size = data->ReparseDataLength + 8;

  if (!DeviceIoControl(source, FSCTL_SET_REPARSE_POINT, &buffer, data_size,
                       NULL, 0, &returned, NULL)) {
    return false;
  }
  return true;
}

// Delete the reparse point referenced by |source|. Returns true if the call
// succeeds, false otherwise.
bool DeleteReparsePoint(HANDLE source) {
  DWORD returned;
  REPARSE_DATA_BUFFER data = {0};
  data.ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
  if (!DeviceIoControl(source, FSCTL_DELETE_REPARSE_POINT, &data, 8, NULL, 0,
                       &returned, NULL)) {
    return false;
  }

  return true;
}

}  // namespace sandbox
