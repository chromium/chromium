// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/event_trace_data.h"

#include <string_view>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_view_util.h"
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
    // SAFETY: `event->MofData` and `event->MofLength` are provided by the
    // Windows ETW subsystem. We trust these values to define the valid memory
    // range for the event payload.
    auto message_span = UNSAFE_BUFFERS(base::span(
        reinterpret_cast<const uint8_t*>(event->MofData), event->MofLength));
    std::string_view message_view = base::as_string_view(message_span);
    data.message.assign(message_view.substr(0, message_view.find('\0')));
  } else if (data.event_type == logging::LOG_MESSAGE_FULL) {
    // SAFETY: `event->MofData` and `event->MofLength` are provided by the
    // Windows ETW subsystem. We trust these values to define the valid memory
    // range for the event payload.
    base::SpanReader reader(UNSAFE_BUFFERS(base::span(
        reinterpret_cast<const uint8_t*>(event->MofData), event->MofLength)));

    // Read the size, skip past the stack info, and move the cursor.
    uint32_t stack_depth;
    if (!reader.ReadU32NativeEndian(stack_depth)) {
      return data;
    }
    base::CheckedNumeric<size_t> bytes_to_skip = stack_depth;
    bytes_to_skip *= sizeof(intptr_t);
    if (!bytes_to_skip.IsValid() || !reader.Skip(bytes_to_skip.ValueOrDie())) {
      return data;
    }

    // Read the line info and move the cursor.
    if (!reader.ReadI32NativeEndian(data.line)) {
      return data;
    }

    // Read the file info and move the cursor.
    std::string_view file_info_view =
        base::as_string_view(reader.remaining_span());
    size_t nul_pos = file_info_view.find('\0');
    if (nul_pos == std::string_view::npos) {
      return data;
    }
    base::FilePath file_path(
        base::UTF8ToWide(file_info_view.substr(0, nul_pos)));
    data.file_name = base::WideToUTF8(file_path.BaseName().value());
    reader.Skip(nul_pos + 1);

    // Read the message and move the cursor.
    std::string_view message_view =
        base::as_string_view(reader.remaining_span());
    nul_pos = message_view.find('\0');
    if (nul_pos == std::string_view::npos) {
      return data;
    }
    data.message.assign(message_view.substr(0, nul_pos));
    reader.Skip(nul_pos + 1);

    // Ensure that the entire buffer was consumed.
    DCHECK_EQ(reader.remaining(), 0u);
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
