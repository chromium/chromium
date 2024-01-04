// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_EVENT_TRACE_DATA_H_
#define REMOTING_HOST_WIN_EVENT_TRACE_DATA_H_

#include <stdint.h>

#include <string>

#include "base/logging.h"
#include "base/logging_win.h"
#include "base/time/time.h"

namespace remoting {

// Generated from an EVENT_TRACE, this structure stores the interesting fields
// in order to simplify event parsing and logging.
struct EventTraceData {
 public:
  // Create an instance from the data in the |event| payload.
  static EventTraceData Create(EVENT_TRACE* event);

  // Helper method to convert a logging severity into a displayable string.
  static std::string SeverityToString(logging::LogSeverity severity);

  EventTraceData(EventTraceData&&);
  EventTraceData& operator=(EventTraceData&&);
  EventTraceData(const EventTraceData&) = delete;
  EventTraceData& operator=(const EventTraceData&) = delete;

  ~EventTraceData();

  // The message type of the event (e.g. LOG_MESSAGE).
  uint8_t event_type = 0;

  // The severity of the event (e.g. warning, info, error).
  logging::LogSeverity severity = logging::LOGGING_INFO;

  // The ID of the process which originally logged the event.
  int process_id = 0;

  // The ID of the thread which originally logged the event.
  int thread_id = 0;

  // The original timestamp from when the event was logged.
  base::Time time_stamp;

  // The name of the file which logged the event.  Note that this may not be
  // present in all builds (it depends on the logging params).
  std::string file_name;

  // The line in the file which logged the event.  Note that this may not be
  // present in all builds (it depends on the logging params).
  int32_t line = 0;

  // The original message which was logged to ETW.
  std::string message;

 private:
  EventTraceData();
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_EVENT_TRACE_DATA_H_
