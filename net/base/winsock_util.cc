// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/winsock_util.h"

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// Pass the important values as function arguments so that they are available
// in crash dumps. Disable inlining so that an actual function call is made and
// disable tail calls so that the parent function is on the call stack.
NOINLINE void NOT_TAIL_CALLED CheckEventWait(WSAEVENT hEvent,
                                             DWORD wait_rv,
                                             DWORD expected) {
  if (wait_rv != expected) {
    DWORD err = ERROR_SUCCESS;
    if (wait_rv == WAIT_FAILED)
      err = GetLastError();
    base::debug::Alias(&err);
    CHECK(false);  // Crash.
  }
}

}  // namespace

void AssertEventNotSignaled(WSAEVENT hEvent) {
  DWORD wait_rv = WaitForSingleObject(hEvent, 0);
  CheckEventWait(hEvent, wait_rv, WAIT_TIMEOUT);
}

bool ResetEventIfSignaled(WSAEVENT hEvent) {
  // TODO(wtc): Remove the CHECKs after enough testing.
  DWORD wait_rv = WaitForSingleObject(hEvent, 0);
  if (wait_rv == WAIT_TIMEOUT)
    return false;  // The event object is not signaled.
  CheckEventWait(hEvent, wait_rv, WAIT_OBJECT_0);
  BOOL ok = WSAResetEvent(hEvent);
  CHECK(ok);
  return true;
}

}  // namespace net
