// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_HOST_EVENT_FILE_LOGGER_H_
#define REMOTING_HOST_WIN_HOST_EVENT_FILE_LOGGER_H_

#include <memory>

#include "base/files/file.h"
#include "remoting/host/win/host_event_logger.h"

namespace remoting {

// Logs ETW event trace data to a file.
class HostEventFileLogger : public HostEventLogger {
 public:
  // Helper function which creates a HostEventLogger instance which will log
  // to a unique file located in the same directory as current executable.
  // Returns nullptr if an error occurs in configuring the logger.
  static std::unique_ptr<HostEventLogger> Create();

  HostEventFileLogger(const HostEventFileLogger&) = delete;
  HostEventFileLogger& operator=(const HostEventFileLogger&) = delete;
  ~HostEventFileLogger() override;

  // HostEventLogger implementation.
  void LogEvent(const EventTraceData& data) override;

 private:
  explicit HostEventFileLogger(base::File log_file);

  base::File log_file_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_HOST_EVENT_FILE_LOGGER_H_
