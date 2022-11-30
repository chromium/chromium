// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/winsock_util.h"

#include "base/check_op.h"

namespace net {

bool ResetEventIfSignaled(WSAEVENT hEvent) {
  DWORD wait_rv = WaitForSingleObject(hEvent, 0);
  if (wait_rv == WAIT_TIMEOUT)
    return false;  // The event object is not signaled.
  DCHECK_EQ(wait_rv, static_cast<DWORD>(WAIT_OBJECT_0));
  BOOL ok = WSAResetEvent(hEvent);
  DCHECK(ok);
  return true;
}

}  // namespace net
