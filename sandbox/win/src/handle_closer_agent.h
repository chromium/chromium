// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_HANDLE_CLOSER_AGENT_H_
#define SANDBOX_WIN_SRC_HANDLE_CLOSER_AGENT_H_

#include <string>

#include "base/win/scoped_handle.h"
#include "sandbox/win/src/handle_closer.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

// Target process code to close the handle list copied over from the broker, and
// deal with disconnecting Csrss.
class HandleCloserAgent {
 public:
  HandleCloserAgent();
  ~HandleCloserAgent();

  HandleCloserAgent(const HandleCloserAgent&) = delete;
  HandleCloserAgent& operator=(const HandleCloserAgent&) = delete;

  // Closes any handles matching those in the lookup map.
  bool CloseHandles();

  // Has the agent closed the connection to csrss.
  bool IsCsrssConnected() { return is_csrss_connected_; }

  // True if we have handles waiting to be closed.
  static bool NeedsHandlesClosed();

 private:
  // Attempt to stuff a closed handle with a dummy Event. Only File and Event
  // handles can be stuffed.
  bool AttemptToStuffHandleSlot(HANDLE closed_handle);
  bool MaybeCloseHandle(std::wstring& type_name, HANDLE handle);

  HandleCloserConfig config_;
  bool is_csrss_connected_;
  base::win::ScopedHandle dummy_handle_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_HANDLE_CLOSER_AGENT_H_
