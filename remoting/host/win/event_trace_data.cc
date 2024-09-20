// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/event_trace_data.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"

namespace remoting {

namespace {

constexpr char kInfoSeverity[] = "INFO";
constexpr char kWarningSeverity[] = "WARNING";
constexpr char kErrorSeverity[] = "ERROR";
constexpr char kFatalSeverity[] = "FATAL";
constexpr char kVerboseSeverity[] = "VERBOSE";

logging::LogSeverity EventTraceLevelToSeverity(uint8_t level) {
  switch (level) {
    case TRACE_LEVEL_NONE:
      NOTREACHED();
    case TRACE_LEVEL_FATAL:
      return logging::LOGGING_FATAL;
    case TRACE_LEVEL_ERROR:
      return logging::LOGGING_ERROR;
    case TRACE_LEVEL_WARNING:
      return logging::LOGGING_WARNING;
    case TRACE_LEVEL_INFORMATION:
      return logging::LOGGING_INFO;
    default:
      // These represent VLOG verbosity levels.
      return TRACE_LEVEL_INFORMATION - level;
  }
}

}  // namespace

EventTraceData::EventTraceData() = default;

EventTraceData::EventTraceData(EventTraceData&&) = default;

EventTraceData& EventTraceData::operator=(EventTraceData&&) = default;

EventTraceData::~EventTraceData() = default;

// static
EventTraceData EventTraceData::Create(EVENT_TRACE* event) {
  EventTraceData data;

  data.event_type = event->Header.Class.Type;

  data.severity = EventTraceLevelToSeverity(event->Header.Class.Level);
  data.process_id = event->Header.ProcessId;
  data.thread_id = event->Header.ThreadId;

  FILETIME event_time = {};
  event_time.dwLowDateTime = event->Header.TimeStamp.LowPart;
  event_time.dwHighDateTime = event->Header.TimeStamp.HighPart;
  data.time_stamp = base::Time::FromFileTime(event_time);

  // Parse the MofData.  The structure is defined in //base/logging_win.cc.
  // - For LOG_MESSAGE events, the MofData buffer just contains the message.
  // - For LOG_MESSAGE_FULL events, the MofData buffer is comprised of 5 fields
  //   which must be parsed (or skipped) in sequence.
  if (data.event_type == logging::LOG_MESSAGE) {
    data.message.assign(reinterpret_cast<const char*>(event->MofData),
                        event->MofLength);
  } else if (data.event_type == logging::LOG_MESSAGE_FULL) {
    const uint8_t* mof_data = reinterpret_cast<const uint8_t*>(event->MofData);
    uint32_t offset = 0;

    // Read the size, skip past the stack info, and move the cursor.
    DWORD stack_depth = *reinterpret_cast<const DWORD*>(mof_data);
    int bytes_to_skip = sizeof(DWORD) + stack_depth * sizeof(intptr_t);
    offset += bytes_to_skip;

    // Read the line info and move the cursor.
    data.line = *reinterpret_cast<const int32_t*>(mof_data + offset);
    offset += sizeof(int32_t);

    // Read the file info and move the cursor.
    const char* file_info = reinterpret_cast<const char*>(mof_data + offset);
    size_t str_len = strnlen_s(file_info, event->MofLength - offset);
    base::FilePath file_path(base::UTF8ToWide(file_info));
    data.file_name = base::WideToUTF8(file_path.BaseName().value());
    offset += (str_len + 1);

    // Read the message and move the cursor.
    const char* message = reinterpret_cast<const char*>(mof_data + offset);
    str_len = strnlen_s(message, event->MofLength - offset);
    data.message.assign(message);
    offset += (str_len + 1);

    DCHECK_EQ(event->MofLength, offset);
  } else {
    NOTREACHED() << "Unknown event type: " << data.event_type;
  }

  return data;
}

// static
std::string EventTraceData::SeverityToString(logging::LogSeverity severity) {
  switch (severity) {
    case logging::LOGGING_INFO:
      return kInfoSeverity;
    case logging::LOGGING_WARNING:
      return kWarningSeverity;
    case logging::LOGGING_ERROR:
      return kErrorSeverity;
    case logging::LOGGING_FATAL:
      return kFatalSeverity;
    default:
      if (severity < 0) {
        return kVerboseSeverity;
      }
      NOTREACHED();
  }
}

}  // namespace remoting
