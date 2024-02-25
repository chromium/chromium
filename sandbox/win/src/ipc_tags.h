// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_IPC_TAGS_H_
#define SANDBOX_WIN_SRC_IPC_TAGS_H_

namespace sandbox {

enum class IpcTag {
  UNUSED = 0,
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
  LAST
};

constexpr size_t kMaxServiceCount = 64;
constexpr size_t kMaxIpcTag = static_cast<size_t>(IpcTag::LAST);
static_assert(kMaxIpcTag <= kMaxServiceCount, "kMaxServiceCount is too low");

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_IPC_TAGS_H_
