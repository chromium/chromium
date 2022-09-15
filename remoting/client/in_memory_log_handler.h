// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IN_MEMORY_LOG_HANDLER_H_
#define REMOTING_CLIENT_IN_MEMORY_LOG_HANDLER_H_

#include <string>

namespace remoting {

// Class for capturing logs in memory before printing out.
class InMemoryLogHandler {
 public:
  InMemoryLogHandler() = delete;
  InMemoryLogHandler(const InMemoryLogHandler&) = delete;
  InMemoryLogHandler& operator=(const InMemoryLogHandler&) = delete;

  // Registers the log handler. This is not thread safe and should be called
  // exactly once in the main function.
  static void Register();

  // Returns most recently captured logs (#lines <= kMaxNumberOfLogs) since the
  // app is launched. This must be called after Register() is called.
  static std::string GetInMemoryLogs();
};

}  // namespace remoting

#endif  //  REMOTING_CLIENT_IN_MEMORY_LOG_HANDLER_H_
