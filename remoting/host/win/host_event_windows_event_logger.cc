// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/host_event_windows_event_logger.h"

#include <utility>

#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "remoting/host/win/event_trace_data.h"
#include "remoting/host/win/remoting_host_messages.h"

constexpr char kApplicationName[] = "chromoting";

namespace remoting {

namespace {

WORD SeverityToEventLogType(logging::LogSeverity severity) {
  // The Windows event log only has 3 log levels so some severity levels will
  // need to be combined (info and verbose, error and fatal).
  switch (severity) {
    case logging::LOGGING_WARNING:
      return EVENTLOG_WARNING_TYPE;
    case logging::LOGGING_ERROR:
    case logging::LOGGING_FATAL:
      // Fatal or Error event.
      return EVENTLOG_ERROR_TYPE;
    case logging::LOGGING_INFO:
    default:
      // Info or Verbose event.
      return EVENTLOG_INFORMATION_TYPE;
  }
}

}  // namespace

HostEventWindowsEventLogger::HostEventWindowsEventLogger(
    WindowsEventLogger event_logger)
    : event_logger_(std::move(event_logger)) {}

HostEventWindowsEventLogger::~HostEventWindowsEventLogger() = default;

void HostEventWindowsEventLogger::LogEvent(const EventTraceData& data) {
  DCHECK(event_logger_.IsRegistered());

  // The first message will be displayed in the general tab in EventViewer.  The
  // additional fields will be logged as part of the EventData which can be
  // viewed in 'details' or queried for in Powershell.
  // Note: The Data elements logged will not have a 'Name' attribute so any
  // scripts which query for these events will need to be updated if the order
  // changes.  This is due to the event logging mechanism we are using.
  // If having a stable query is important in the future, then we will need to
  // change from a MOF based provider to a manifest based provider and define an
  // event schema.
  base::Time::Exploded exploded;
  data.time_stamp.LocalExplode(&exploded);
  std::vector<std::string> payload(
      {data.message, "pid: " + base::NumberToString(data.process_id),
       "tid: " + base::NumberToString(data.thread_id),
       EventTraceData::SeverityToString(data.severity),
       base::StringPrintf("%s(%d)", data.file_name.c_str(), data.line),
       base::UnlocalizedTimeFormatWithPattern(data.time_stamp,
                                              "yyyy-MM-dd - HH:mm:ss.SSS")});

  WORD type = SeverityToEventLogType(data.severity);
  if (!event_logger_.Log(type, MSG_HOST_LOG_EVENT, payload)) {
    // Don't log a write error more than once.  We don't want to create a
    // loopback effect and fill up the log for other active event loggers.
    static bool logged_once = false;
    // The LOG statement below should get pushed to the Windows ETW queue and
    // will be delivered to the event trace consumer asynchronously.  Just to be
    // safe, we store and then update the static value to prevent any potential
    // reentrancy issues.
    bool local_logged_once = logged_once;
    logged_once = true;
    PLOG_IF(ERROR, !local_logged_once) << "Failed to write to the event log";
  }
}

// static.
std::unique_ptr<HostEventLogger> HostEventWindowsEventLogger::Create() {
  WindowsEventLogger event_logger(kApplicationName);
  if (!event_logger.IsRegistered()) {
    return nullptr;
  }

  return std::make_unique<HostEventWindowsEventLogger>(std::move(event_logger));
}

}  // namespace remoting
