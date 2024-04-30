// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_IPC_TAGS_H_
#define SANDBOX_WIN_SRC_IPC_TAGS_H_

#include <cstdint>

namespace sandbox {

enum class IpcTag : uint32_t {
  UNUSED,
  PING1,  // Takes a cookie in parameters and returns the cookie
          // multiplied by 2 and the tick_count. Used for testing only.
  PING2,  // Takes an in/out cookie in parameters and modify the cookie
          // to be multiplied by 3. Used for testing only.
  NTCREATEFILE,
  NTOPENFILE,
  NTQUERYATTRIBUTESFILE,
  NTQUERYFULLATTRIBUTESFILE,
  NTSETINFO_RENAME,
  NTOPENTHREAD,
  NTOPENPROCESSTOKENEX,
  GDI_GDIDLLINITIALIZE,
  GDI_GETSTOCKOBJECT,
  USER_REGISTERCLASSW,
  CREATETHREAD,
  NTCREATESECTION,
  kMaxValue = NTCREATESECTION,
};

// The number of IpcTag services that are defined.
inline constexpr size_t kSandboxIpcCount =
    static_cast<size_t>(IpcTag::kMaxValue) + 1;
}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_IPC_TAGS_H_
