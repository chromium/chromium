// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/wts_terminal_monitor.h"

#include <windows.h>

#include <wtsapi32.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace remoting {

// Session id that does not represent any session.
const uint32_t kInvalidSessionId = 0xffffffffu;

const char WtsTerminalMonitor::kConsole[] = "console";

WtsTerminalMonitor::~WtsTerminalMonitor() = default;

// static
bool WtsTerminalMonitor::LookupTerminalId(uint32_t session_id,
                                          std::string* terminal_id) {
  // Fast path for the case when `session_id` is currently attached to
  // the physical console.
  if (session_id == WTSGetActiveConsoleSessionId()) {
    *terminal_id = kConsole;
    return true;
  }

  // RdpClient sets the terminal ID as the initial program's working directory.
  DWORD bytes;
  wchar_t* working_directory;
  if (!WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, session_id,
                                  WTSWorkingDirectory, &working_directory,
                                  &bytes)) {
    return false;
  }

  absl::Cleanup wts_deleter = [working_directory] {
    ::WTSFreeMemory(working_directory);
  };

  if (!working_directory || bytes < sizeof(wchar_t)) {
    return false;
  }

  std::string id;
  if (!base::WideToUTF8(working_directory, (bytes / sizeof(wchar_t)) - 1,
                        &id)) {
    return false;
  }

  // The working directory is controlled by the client of the RDP connection,
  // so it identifies a terminal only if it carries a virtual terminal ID
  // assigned by RdpClient. In particular, a session that is not attached to
  // the physical console must never be reported as the console terminal.
  if (!IsVirtualTerminalId(id)) {
    return false;
  }

  *terminal_id = std::move(id);
  return true;
}

// static
uint32_t WtsTerminalMonitor::LookupSessionId(const std::string& terminal_id) {
  // Use the fast path if the caller wants to get id of the session attached to
  // the physical console.
  if (terminal_id == kConsole) {
    return WTSGetActiveConsoleSessionId();
  }

  // Enumerate all sessions and try to match the client endpoint.
  WTS_SESSION_INFO* session_info;
  DWORD session_info_count;
  if (!WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &session_info,
                            &session_info_count)) {
    PLOG(ERROR) << "Failed to enumerate all sessions";
    return kInvalidSessionId;
  }

  absl::Cleanup wts_deleter = [session_info] { ::WTSFreeMemory(session_info); };
  for (DWORD i = 0; i < session_info_count; ++i) {
    uint32_t session_id = UNSAFE_TODO(session_info[i]).SessionId;

    std::string id;
    if (LookupTerminalId(session_id, &id) && terminal_id == id) {
      return session_id;
    }
  }

  // `terminal_id` is not associated with any session.
  return kInvalidSessionId;
}

// static
std::string WtsTerminalMonitor::GenerateVirtualTerminalId() {
  return base::Uuid::GenerateRandomV4().AsLowercaseString();
}

// static
bool WtsTerminalMonitor::IsVirtualTerminalId(const std::string& terminal_id) {
  return base::Uuid::ParseLowercase(terminal_id).is_valid();
}

WtsTerminalMonitor::WtsTerminalMonitor() = default;

}  // namespace remoting
