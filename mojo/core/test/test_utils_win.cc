// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/test/test_utils.h"

#include <windows.h>

#include <fcntl.h>
#include <io.h>
#include <stddef.h>
#include <string.h>

#include <ostream>

namespace mojo {
namespace core {
namespace test {

PlatformHandle PlatformHandleFromFILE(base::ScopedFILE fp) {
  CHECK(fp);

  HANDLE rv = INVALID_HANDLE_VALUE;
  PCHECK(DuplicateHandle(
      GetCurrentProcess(),
      reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(fp.get()))),
      GetCurrentProcess(), &rv, 0, TRUE, DUPLICATE_SAME_ACCESS))
      << "DuplicateHandle";
  return PlatformHandle(base::win::ScopedHandle(rv));
}

base::ScopedFILE FILEFromPlatformHandle(PlatformHandle h, const char* mode) {
  CHECK(h.is_valid());
  // Microsoft's documentation for |_open_osfhandle()| only discusses these
  // flags (and |_O_WTEXT|). Hmmm.
  int flags = 0;
  if (strchr(mode, 'a'))
    flags |= _O_APPEND;
  if (strchr(mode, 'r'))
    flags |= _O_RDONLY;
  if (strchr(mode, 't'))
    flags |= _O_TEXT;
  base::ScopedFILE rv(_fdopen(
      _open_osfhandle(reinterpret_cast<intptr_t>(h.ReleaseHandle()), flags),
      mode));
  PCHECK(rv) << "_fdopen";
  return rv;
}

}  // namespace test
}  // namespace core
}  // namespace mojo
