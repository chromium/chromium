// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_LOG_TO_SERVER_H_
#define REMOTING_SIGNALING_LOG_TO_SERVER_H_

#include "remoting/signaling/server_log_entry.h"

namespace remoting {

// LogToServer sends log entries to a server.
// The contents of the log entries are described in server_log_entry.cc.
// They do not contain any personally identifiable information.
class LogToServer {
 public:
  virtual ~LogToServer() = default;

  virtual void Log(const ServerLogEntry& entry) = 0;
  virtual ServerLogEntry::Mode mode() const = 0;

 protected:
  LogToServer() = default;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_LOG_TO_SERVER_H_
