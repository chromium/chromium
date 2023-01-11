// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SCOPED_SC_HANDLE_WIN_H_
#define REMOTING_BASE_SCOPED_SC_HANDLE_WIN_H_

#include <windows.h>

#include "base/win/scoped_handle.h"

namespace remoting {

class ScHandleTraits {
 public:
  typedef SC_HANDLE Handle;

  ScHandleTraits() = delete;
  ScHandleTraits(const ScHandleTraits&) = delete;
  ScHandleTraits& operator=(const ScHandleTraits&) = delete;

  // Closes the handle.
  static bool CloseHandle(SC_HANDLE handle) {
    return ::CloseServiceHandle(handle) != FALSE;
  }

  // Returns true if the handle value is valid.
  static bool IsHandleValid(SC_HANDLE handle) { return handle != NULL; }

  // Returns NULL handle value.
  static SC_HANDLE NullHandle() { return NULL; }
};

typedef base::win::GenericScopedHandle<ScHandleTraits,
                                       base::win::DummyVerifierTraits>
    ScopedScHandle;

}  // namespace remoting

#endif  // REMOTING_BASE_SCOPED_SC_HANDLE_WIN_H_
