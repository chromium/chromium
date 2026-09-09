// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/wts_terminal_monitor.h"

#include <windows.h>

#include <wtsapi32.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace remoting {

TEST(WtsTerminalMonitorTest, GeneratedVirtualTerminalIdsAreValid) {
  EXPECT_TRUE(WtsTerminalMonitor::IsVirtualTerminalId(
      WtsTerminalMonitor::GenerateVirtualTerminalId()));
}

TEST(WtsTerminalMonitorTest, GeneratedVirtualTerminalIdsAreUnique) {
  EXPECT_NE(WtsTerminalMonitor::GenerateVirtualTerminalId(),
            WtsTerminalMonitor::GenerateVirtualTerminalId());
}

// The console terminal ID identifies the session that is attached to the
// physical console, so it must never be accepted as a virtual terminal ID.
TEST(WtsTerminalMonitorTest, ConsoleTerminalIdIsNotAVirtualTerminalId) {
  EXPECT_FALSE(
      WtsTerminalMonitor::IsVirtualTerminalId(WtsTerminalMonitor::kConsole));
}

// Virtual terminal IDs are read back from the working directory that RdpClient
// specifies when connecting. Working directory values that RdpClient does not
// produce do not identify a virtual terminal.
class WtsTerminalMonitorInvalidIdTest
    : public testing::TestWithParam<const char*> {};

TEST_P(WtsTerminalMonitorInvalidIdTest, IsNotVirtualTerminalId) {
  EXPECT_FALSE(WtsTerminalMonitor::IsVirtualTerminalId(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    InvalidVirtualTerminalIds,
    WtsTerminalMonitorInvalidIdTest,
    testing::Values("",
                    "Console",
                    "C:\\Users\\Default",
                    "%HOMEDRIVE%%HOMEPATH%\\console",
                    // Uppercase UUIDs are rejected.
                    "D3B07384-D113-467F-94D7-E07E868A8677",
                    // Braced UUIDs are rejected.
                    "{d3b07384-d113-467f-94d7-e07e868a8677}",
                    // UUIDs without hyphens are rejected.
                    "d3b07384d113467f94d7e07e868a8677",
                    // UUIDs with leading or trailing whitespace are rejected.
                    " d3b07384-d113-467f-94d7-e07e868a8677",
                    "d3b07384-d113-467f-94d7-e07e868a8677 "));

// The session attached to the physical console is reported as the console
// terminal.
TEST(WtsTerminalMonitorTest, ActiveConsoleSessionMapsToConsoleTerminal) {
  uint32_t console_session_id = WTSGetActiveConsoleSessionId();
  // WTSGetActiveConsoleSessionId() returns the ID of the session attached to
  // the physical console. This is expected to execute on CQ try-bots and
  // during local interactive desktop runs. However, it returns
  // kInvalidSessionId when executed within an RDP session or in a headless
  // environment without an attached physical console, in which case the test
  // is skipped.
  if (console_session_id == kInvalidSessionId) {
    GTEST_SKIP() << "No session is attached to the physical console.";
  }

  std::string terminal_id;
  ASSERT_TRUE(
      WtsTerminalMonitor::LookupTerminalId(console_session_id, &terminal_id));
  EXPECT_EQ(terminal_id, WtsTerminalMonitor::kConsole);
}

// The console terminal always resolves to the session attached to the physical
// console.
TEST(WtsTerminalMonitorTest, ConsoleTerminalMapsToActiveConsoleSession) {
  EXPECT_EQ(WtsTerminalMonitor::LookupSessionId(WtsTerminalMonitor::kConsole),
            WTSGetActiveConsoleSessionId());
}

// A session that is not attached to the physical console must never be
// reported as the console terminal, and may only be identified by the virtual
// terminal ID assigned to it.
TEST(WtsTerminalMonitorTest, NonConsoleSessionsDoNotMapToConsoleTerminal) {
  uint32_t console_session_id = WTSGetActiveConsoleSessionId();

  WTS_SESSION_INFO* session_info;
  DWORD session_info_count;
  ASSERT_TRUE(WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1,
                                   &session_info, &session_info_count));
  absl::Cleanup wts_deleter = [session_info] { ::WTSFreeMemory(session_info); };

  // SAFETY: WTSEnumerateSessions() allocated `session_info_count` entries.
  auto sessions = UNSAFE_BUFFERS(
      base::span<const WTS_SESSION_INFO>(session_info, session_info_count));
  for (const WTS_SESSION_INFO& session : sessions) {
    if (session.SessionId == console_session_id) {
      continue;
    }

    std::string terminal_id;
    if (WtsTerminalMonitor::LookupTerminalId(session.SessionId, &terminal_id)) {
      EXPECT_TRUE(WtsTerminalMonitor::IsVirtualTerminalId(terminal_id))
          << "Session " << session.SessionId
          << " reported terminal ID: " << terminal_id;
    }
  }
}

// A virtual terminal that no RDP connection was created for is not associated
// with any session.
TEST(WtsTerminalMonitorTest, UnknownVirtualTerminalIdMapsToNoSession) {
  EXPECT_EQ(WtsTerminalMonitor::LookupSessionId(
                WtsTerminalMonitor::GenerateVirtualTerminalId()),
            kInvalidSessionId);
}

// The empty string is not a terminal ID, so it must not resolve to a session
// even if a session reports an empty working directory.
TEST(WtsTerminalMonitorTest, EmptyTerminalIdMapsToNoSession) {
  EXPECT_EQ(WtsTerminalMonitor::LookupSessionId(std::string()),
            kInvalidSessionId);
}

}  // namespace remoting
