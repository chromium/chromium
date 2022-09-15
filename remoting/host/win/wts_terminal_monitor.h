// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WTS_TERMINAL_MONITOR_H_
#define REMOTING_HOST_WIN_WTS_TERMINAL_MONITOR_H_

#include <stdint.h>

#include <string>

#include "base/strings/utf_string_conversions.h"

namespace remoting {

class WtsTerminalObserver;

// Session id that does not represent any session.
extern const uint32_t kInvalidSessionId;

class WtsTerminalMonitor {
 public:
  // The console terminal ID.
  static const char kConsole[];

  WtsTerminalMonitor(const WtsTerminalMonitor&) = delete;
  WtsTerminalMonitor& operator=(const WtsTerminalMonitor&) = delete;

  virtual ~WtsTerminalMonitor();

  // Registers an observer to receive notifications about a particular WTS
  // terminal. |terminal_id| is used to specify an RdpClient instance for which
  // the connected session should be monitored, or |kConsole| may be passed to
  // monitor the console session.
  //
  // Each observer instance can monitor a single WTS console. Returns
  // |true| of success. Returns |false| if |observer| is already registered.
  virtual bool AddWtsTerminalObserver(const std::string& terminal_id,
                                      WtsTerminalObserver* observer) = 0;

  // Unregisters a previously registered observer.
  virtual void RemoveWtsTerminalObserver(WtsTerminalObserver* observer) = 0;

  // Returns ID of the terminal connected to |session_id| in |*terminal_id|.
  // Returns false if |session_id| is not attached to the physical console or
  // does not have an assigned terminal ID.
  static bool LookupTerminalId(uint32_t session_id, std::string* terminal_id);

  // Returns ID of the session that |terminal_id| is attached.
  // |kInvalidSessionId| is returned if none of the sessions is currently
  // attahced to |client_endpoint|.
  static uint32_t LookupSessionId(const std::string& terminal_id);

 protected:
  WtsTerminalMonitor();
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WTS_TERMINAL_MONITOR_H_
