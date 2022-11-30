// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_HOST_EVENT_WINDOWS_EVENT_LOGGER_H_
#define REMOTING_HOST_WIN_HOST_EVENT_WINDOWS_EVENT_LOGGER_H_

#include <windows.h>

#include <memory>

#include "remoting/host/win/host_event_logger.h"
#include "remoting/host/win/windows_event_logger.h"

namespace remoting {

// Logs ETW event trace data to the Windows event log.
class HostEventWindowsEventLogger : public HostEventLogger {
 public:
  // Helper function which creates a HostEventLogger instance which will log
  // to the Windows event log.  Returns nullptr if an error occurs while
  // configuring the logger.
  static std::unique_ptr<HostEventLogger> Create();

  explicit HostEventWindowsEventLogger(WindowsEventLogger event_logger);
  HostEventWindowsEventLogger(const HostEventWindowsEventLogger&) = delete;
  HostEventWindowsEventLogger& operator=(const HostEventWindowsEventLogger&) =
      delete;
  ~HostEventWindowsEventLogger() override;

  // HostEventLogger implementation.
  void LogEvent(const EventTraceData& data) override;

 private:
  WindowsEventLogger event_logger_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_HOST_EVENT_WINDOWS_EVENT_LOGGER_H_
