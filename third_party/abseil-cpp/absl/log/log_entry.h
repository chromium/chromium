// Copyright 2022 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: log/log_entry.h
// -----------------------------------------------------------------------------
//
// This header declares `class absl::LogEntry`, which represents a log record as
// passed to `LogSink::Send`. Data returned by pointer or by reference or by
// `absl::string_view` must be copied if they are needed after the lifetime of
// the `absl::LogEntry`.

#ifndef ABSL_LOG_LOG_ENTRY_H_
#define ABSL_LOG_LOG_ENTRY_H_

#include <cstddef>
#include <string>

#include "absl/base/config.h"
#include "absl/base/log_severity.h"
#include "absl/log/internal/config.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

namespace log_internal {
// Test only friend.
class LogEntryTestPeer;
class LogMessage;
}  // namespace log_internal

// LogEntry
//
// Represents a single entry in a log, i.e., one log message.
//
// LogEntry is copyable and thread-compatible.
class LogEntry final {
 public:
  using tid_t = log_internal::Tid;

  // For non-verbose log entries, `verbosity()` returns `kNoVerbosityLevel`.
  static constexpr int kNoVerbosityLevel = -1;
  static constexpr int kNoVerboseLevel = -1;  // TO BE removed

  LogEntry(const LogEntry&) = default;
  LogEntry& operator=(const LogEntry&) = default;

  // Source file and line where the log message occurred.
  // Take special care not to dereference the pointers returned by
  // source_filename() and source_basename() after the lifetime of the
  // `LogEntry`. This will usually work, because these are usually backed by a
  // statically allocated char array obtained from the `__FILE__` macro, but
  // it is nevertheless incorrect and will be broken by statements like
  // `LOG(INFO).AtLocation(...)` (see above).  If you need the data later, you
  // must copy it.
  absl::string_view source_filename() const { return full_filename_; }
  absl::string_view source_basename() const { return base_filename_; }
  int source_line() const { return line_; }

  // LogEntry::prefix()
  //
  // True unless cleared by LOG(...).NoPrefix(), which indicates suppression of
  // the line prefix containing metadata like file, line, timestamp, etc.
  bool prefix() const { return prefix_; }

  // LogEntry::log_severity()
  //
  // Returns this LogEntry's severity.
  absl::LogSeverity log_severity() const { return severity_; }

  // LogEntry::verbosity()
  //
  // Returns this LogEntry's verbosity, or kNoVerbosityLevel for a non-verbose
  // LogEntry.
  int verbosity() const { return verbose_level_; }

  // LogEntry::timestamp()
  //
  // Returns the time at which this LogEntry was written.
  absl::Time timestamp() const { return timestamp_; }

  // LogEntry::tid()
  //
  // Returns the id of the thread that wrote this LogEntry.
  tid_t tid() const { return tid_; }

  // Text-formatted version of the log message.  An underlying buffer holds:
  //
  // * A prefix formed by formatting metadata (timestamp, filename, line number,
  //   etc.)
  // * The streamed data
  // * A newline
  // * A nul terminator
  //
  // These methods give access to the most commonly-used substrings of the
  // buffer's contents.  Other combinations can be obtained with substring
  // arithmetic.
  absl::string_view text_message_with_prefix_and_newline() const {
    return absl::string_view(
        text_message_with_prefix_and_newline_and_nul_.data(),
        text_message_with_prefix_and_newline_and_nul_.size() - 1);
  }
  absl::string_view text_message_with_prefix() const {
    return absl::string_view(
        text_message_with_prefix_and_newline_and_nul_.data(),
        text_message_with_prefix_and_newline_and_nul_.size() - 2);
  }
  absl::string_view text_message_with_newline() const {
    return absl::string_view(
        text_message_with_prefix_and_newline_and_nul_.data() + prefix_len_,
        text_message_with_prefix_and_newline_and_nul_.size() - prefix_len_ - 1);
  }
  absl::string_view text_message() const {
    return absl::string_view(
        text_message_with_prefix_and_newline_and_nul_.data() + prefix_len_,
        text_message_with_prefix_and_newline_and_nul_.size() - prefix_len_ - 2);
  }
  const char* text_message_with_prefix_and_newline_c_str() const {
    return text_message_with_prefix_and_newline_and_nul_.data();
  }

  // LogEntry::stacktrace()
  //
  // Optional stacktrace, e.g., for `FATAL` logs.
  absl::string_view stacktrace() const { return stacktrace_; }

 private:
  LogEntry() = default;

  absl::string_view full_filename_;
  absl::string_view base_filename_;
  int line_;
  bool prefix_;
  absl::LogSeverity severity_;
  int verbose_level_;  // >=0 for `VLOG`, etc.; otherwise `kNoVerbosityLevel`.
  absl::Time timestamp_;
  tid_t tid_;
  absl::Span<const char> text_message_with_prefix_and_newline_and_nul_;
  size_t prefix_len_;
  std::string stacktrace_;

  friend class log_internal::LogEntryTestPeer;
  friend class log_internal::LogMessage;
};

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_LOG_LOG_ENTRY_H_
