// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_HANDLE_CLOSER_AGENT_H_
#define SANDBOX_SRC_HANDLE_CLOSER_AGENT_H_

#include <string>

#include "base/macros.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/handle_closer.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/target_services.h"

namespace sandbox {

// Target process code to close the handle list copied over from the broker.
class HandleCloserAgent {
 public:
  HandleCloserAgent();
  ~HandleCloserAgent();

  // Reads the serialized list from the broker and creates the lookup map.
  // Updates is_csrss_connected based on type of handles closed.
  void InitializeHandlesToClose(bool* is_csrss_connected);

  // Closes any handles matching those in the lookup map.
  bool CloseHandles();

  // True if we have handles waiting to be closed.
  static bool NeedsHandlesClosed();

 private:
  // Attempt to stuff a closed handle with a dummy Event.
  bool AttemptToStuffHandleSlot(HANDLE closed_handle, const std::wstring& type);

  HandleMap handles_to_close_;
  base::win::ScopedHandle dummy_handle_;

  DISALLOW_COPY_AND_ASSIGN(HandleCloserAgent);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_HANDLE_CLOSER_AGENT_H_
