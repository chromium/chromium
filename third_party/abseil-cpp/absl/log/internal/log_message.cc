//
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

#include "absl/log/internal/log_message.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <ostream>
#include <string>
#include <tuple>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/base/internal/strerror.h"
#include "absl/base/internal/sysinfo.h"
#include "absl/base/log_severity.h"
#include "absl/container/inlined_vector.h"
#include "absl/debugging/internal/examine_stack.h"
#include "absl/log/globals.h"
#include "absl/log/internal/config.h"
#include "absl/log/internal/globals.h"
#include "absl/log/internal/log_format.h"
#include "absl/log/internal/log_sink_set.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"
#include "absl/log/log_sink_registry.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"

extern "C" ABSL_ATTRIBUTE_WEAK void ABSL_INTERNAL_C_SYMBOL(
    AbslInternalOnFatalLogMessage)(const absl::LogEntry&) {
  // Default - Do nothing
}

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace log_internal {

namespace {
// Copies into `dst` as many bytes of `src` as will fit, then truncates the
// copied bytes from the front of `dst` and returns the number of bytes written.
size_t AppendTruncated(absl::string_view src, absl::Span<char>* dst) {
  if (src.size() > dst->size()) src = src.substr(0, dst->size());
  memcpy(dst->data(), src.data(), src.size());
  dst->remove_prefix(src.size());
  return src.size();
}

absl::string_view Basename(absl::string_view filepath) {
#ifdef _WIN32
  size_t path = filepath.find_last_of("/\\");
#else
  size_t path = filepath.find_last_of('/');
#endif
  if (path != filepath.npos) filepath.remove_prefix(path + 1);
  return filepath;
}

void WriteToString(const char* data, void* str) {
  reinterpret_cast<std::string*>(str)->append(data);
}
void WriteToStream(const char* data, void* os) {
  auto* cast_os = static_cast<std::ostream*>(os);
  *cast_os << data;
}
}  // namespace

// A write-only `std::streambuf` that writes into an `absl::Span<char>`.
//
// This class is responsible for writing a metadata prefix just before the first
// data are streamed in.  The metadata are subject to change (cf.
// `LogMessage::AtLocation`) until then, so we wait as long as possible.
//
// This class is also responsible for reserving space for a trailing newline
// so that one can be added later by `Finalize` no matter how many data are
// streamed in.
class LogEntryStreambuf final : public std::streambuf {
 public:
  explicit LogEntryStreambuf(absl::Span<char> buf, const absl::LogEntry& entry)
      : buf_(buf), entry_(entry), prefix_len_(0), finalized_(false) {
    // To detect when data are first written, we leave the put area null,
    // override `overflow`, and check ourselves in `xsputn`.
  }

  LogEntryStreambuf(LogEntryStreambuf&&) = delete;
  LogEntryStreambuf& operator=(LogEntryStreambuf&&) = delete;

  absl::Span<const char> Finalize() {
    assert(!finalized_);
    // If no data were ever streamed in, this is where we must write the prefix.
    if (pbase() == nullptr) Initialize();
    // Here we reclaim the two bytes we reserved.
    ptrdiff_t idx = pptr() - pbase();
    setp(buf_.data(), buf_.data() + buf_.size());
    pbump(static_cast<int>(idx));
    sputc('\n');
    sputc('\0');
    finalized_ = true;
    return absl::Span<const char>(pbase(),
                                  static_cast<size_t>(pptr() - pbase()));
  }
  size_t prefix_len() const { return prefix_len_; }

 protected:
  std::streamsize xsputn(const char* s, std::streamsize n) override {
    if (n < 0) return 0;
    if (pbase() == nullptr) Initialize();
    return static_cast<std::streamsize>(
        Append(absl::string_view(s, static_cast<size_t>(n))));
  }

  int overflow(int ch = EOF) override {
    if (pbase() == nullptr) Initialize();
    if (ch == EOF) return 0;
    if (pptr() == epptr()) return EOF;
    *pptr() = static_cast<char>(ch);
    pbump(1);
    return 1;
  }

 private:
  void Initialize() {
    // Here we reserve two bytes in our buffer to guarantee `Finalize` space to
    // add a trailing "\n\0".
    assert(buf_.size() >= 2);
    setp(buf_.data(), buf_.data() + buf_.size() - 2);
    if (entry_.prefix()) {
      absl::Span<char> remaining = buf_;
      prefix_len_ = log_internal::FormatLogPrefix(
          entry_.log_severity(), entry_.timestamp(), entry_.tid(),
          entry_.source_basename(), entry_.source_line(), remaining);
      pbump(static_cast<int>(prefix_len_));
    }
  }

  size_t Append(absl::string_view data) {
    absl::Span<char> remaining(pptr(), static_cast<size_t>(epptr() - pptr()));
    const size_t written = AppendTruncated(data, &remaining);
    pbump(static_cast<int>(written));
    return written;
  }

  const absl::Span<char> buf_;
  const absl::LogEntry& entry_;
  size_t prefix_len_;
  bool finalized_;
};

struct LogMessage::LogMessageData final {
  LogMessageData(const char* file, int line, absl::LogSeverity severity,
                 absl::Time timestamp);
  LogMessageData(const LogMessageData&) = delete;
  LogMessageData& operator=(const LogMessageData&) = delete;

  // `LogEntry` sent to `LogSink`s; contains metadata.
  absl::LogEntry entry;

  // true => this was first fatal msg
  bool first_fatal;
  // true => all failures should be quiet
  bool fail_quietly;
  // true => PLOG was requested
  bool is_perror;

  // Extra `LogSink`s to log to, in addition to `global_sinks`.
  absl::InlinedVector<absl::LogSink*, 16> extra_sinks;
  // If true, log to `extra_sinks` but not to `global_sinks` or hardcoded
  // non-sink targets (e.g. stderr, log files).
  bool extra_sinks_only;

  // A formatted string message is built in `string_buf`.
  std::array<char, kLogMessageBufferSize> string_buf;

  // A `std::streambuf` that stores into `string_buf`.
  LogEntryStreambuf streambuf_;
};

LogMessage::LogMessageData::LogMessageData(const char* file, int line,
                                           absl::LogSeverity severity,
                                           absl::Time timestamp)
    : extra_sinks_only(false),
      streambuf_(absl::MakeSpan(string_buf), entry) {
  entry.full_filename_ = file;
  entry.base_filename_ = Basename(file);
  entry.line_ = line;
  entry.prefix_ = absl::ShouldPrependLogPrefix();
  entry.severity_ = absl::NormalizeLogSeverity(severity);
  entry.verbose_level_ = absl::LogEntry::kNoVerbosityLevel;
  entry.timestamp_ = timestamp;
  entry.tid_ = absl::base_internal::GetCachedTID();
}

LogMessage::LogMessage(const char* file, int line, absl::LogSeverity severity)
    : data_(
          absl::make_unique<LogMessageData>(file, line, severity, absl::Now()))
      ,
      stream_(&data_->streambuf_)
{
  data_->first_fatal = false;
  data_->is_perror = false;
  data_->fail_quietly = false;

  // Legacy defaults for LOG's ostream:
  stream_.setf(std::ios_base::showbase | std::ios_base::boolalpha);
  // `fill('0')` is omitted here because its effects are very different without
  // structured logging.  Resolution is tracked in b/111310488.

  // This logs a backtrace even if the location is subsequently changed using
  // AtLocation.  This quirk, and the behavior when AtLocation is called twice,
  // are fixable but probably not worth fixing.
  LogBacktraceIfNeeded();
}

LogMessage::~LogMessage() {
#ifdef ABSL_MIN_LOG_LEVEL
  if (data_->entry.log_severity() <
          static_cast<absl::LogSeverity>(ABSL_MIN_LOG_LEVEL) &&
      data_->entry.log_severity() < absl::LogSeverity::kFatal) {
    return;
  }
#endif
  Flush();
}

LogMessage& LogMessage::AtLocation(absl::string_view file, int line) {
  data_->entry.full_filename_ = file;
  data_->entry.base_filename_ = Basename(file);
  data_->entry.line_ = line;
  LogBacktraceIfNeeded();
  return *this;
}

LogMessage& LogMessage::NoPrefix() {
  data_->entry.prefix_ = false;
  return *this;
}

LogMessage& LogMessage::WithVerbosity(int verbose_level) {
  if (verbose_level == absl::LogEntry::kNoVerbosityLevel) {
    data_->entry.verbose_level_ = absl::LogEntry::kNoVerbosityLevel;
  } else {
    data_->entry.verbose_level_ = std::max(0, verbose_level);
  }
  return *this;
}

LogMessage& LogMessage::WithTimestamp(absl::Time timestamp) {
  data_->entry.timestamp_ = timestamp;
  return *this;
}

LogMessage& LogMessage::WithThreadID(absl::LogEntry::tid_t tid) {
  data_->entry.tid_ = tid;
  return *this;
}

LogMessage& LogMessage::WithMetadataFrom(const absl::LogEntry& entry) {
  data_->entry.full_filename_ = entry.full_filename_;
  data_->entry.base_filename_ = entry.base_filename_;
  data_->entry.line_ = entry.line_;
  data_->entry.prefix_ = entry.prefix_;
  data_->entry.severity_ = entry.severity_;
  data_->entry.verbose_level_ = entry.verbose_level_;
  data_->entry.timestamp_ = entry.timestamp_;
  data_->entry.tid_ = entry.tid_;
  return *this;
}

LogMessage& LogMessage::WithPerror() {
  data_->is_perror = true;
  return *this;
}

LogMessage& LogMessage::ToSinkAlso(absl::LogSink* sink) {
  ABSL_INTERNAL_CHECK(sink, "null LogSink*");
  data_->extra_sinks.push_back(sink);
  return *this;
}

LogMessage& LogMessage::ToSinkOnly(absl::LogSink* sink) {
  ABSL_INTERNAL_CHECK(sink, "null LogSink*");
  data_->extra_sinks.clear();
  data_->extra_sinks.push_back(sink);
  data_->extra_sinks_only = true;
  return *this;
}

#ifdef __ELF__
extern "C" void __gcov_dump() ABSL_ATTRIBUTE_WEAK;
extern "C" void __gcov_flush() ABSL_ATTRIBUTE_WEAK;
#endif

void LogMessage::FailWithoutStackTrace() {
  // Now suppress repeated trace logging:
  log_internal::SetSuppressSigabortTrace(true);
#if defined _DEBUG && defined COMPILER_MSVC
  // When debugging on windows, avoid the obnoxious dialog.
  __debugbreak();
#endif

#ifdef __ELF__
  // For b/8737634, flush coverage if we are in coverage mode.
  if (&__gcov_dump != nullptr) {
    __gcov_dump();
  } else if (&__gcov_flush != nullptr) {
    __gcov_flush();
  }
#endif

  abort();
}

void LogMessage::FailQuietly() {
  // _exit. Calling abort() would trigger all sorts of death signal handlers
  // and a detailed stack trace. Calling exit() would trigger the onexit
  // handlers, including the heap-leak checker, which is guaranteed to fail in
  // this case: we probably just new'ed the std::string that we logged.
  // Anyway, if you're calling Fail or FailQuietly, you're trying to bail out
  // of the program quickly, and it doesn't make much sense for FailQuietly to
  // offer different guarantees about exit behavior than Fail does. (And as a
  // consequence for QCHECK and CHECK to offer different exit behaviors)
  _exit(1);
}

template LogMessage& LogMessage::operator<<(const char& v);
template LogMessage& LogMessage::operator<<(const signed char& v);
template LogMessage& LogMessage::operator<<(const unsigned char& v);
template LogMessage& LogMessage::operator<<(const short& v);           // NOLINT
template LogMessage& LogMessage::operator<<(const unsigned short& v);  // NOLINT
template LogMessage& LogMessage::operator<<(const int& v);
template LogMessage& LogMessage::operator<<(const unsigned int& v);
template LogMessage& LogMessage::operator<<(const long& v);           // NOLINT
template LogMessage& LogMessage::operator<<(const unsigned long& v);  // NOLINT
template LogMessage& LogMessage::operator<<(const long long& v);      // NOLINT
template LogMessage& LogMessage::operator<<(
    const unsigned long long& v);  // NOLINT
template LogMessage& LogMessage::operator<<(void* const& v);
template LogMessage& LogMessage::operator<<(const void* const& v);
template LogMessage& LogMessage::operator<<(const float& v);
template LogMessage& LogMessage::operator<<(const double& v);
template LogMessage& LogMessage::operator<<(const bool& v);
template LogMessage& LogMessage::operator<<(const std::string& v);
template LogMessage& LogMessage::operator<<(const absl::string_view& v);

void LogMessage::Flush() {
  if (data_->entry.log_severity() < absl::MinLogLevel())
    return;

  if (data_->is_perror) {
    InternalStream() << ": " << absl::base_internal::StrError(errno_saver_())
                     << " [" << errno_saver_() << "]";
  }

  // Have we already seen a fatal message?
  ABSL_CONST_INIT static std::atomic_flag seen_fatal = ATOMIC_FLAG_INIT;
  if (data_->entry.log_severity() == absl::LogSeverity::kFatal &&
      absl::log_internal::ExitOnDFatal()) {
    // Exactly one LOG(FATAL) message is responsible for aborting the process,
    // even if multiple threads LOG(FATAL) concurrently.
    data_->first_fatal = !seen_fatal.test_and_set(std::memory_order_relaxed);
  }

  data_->entry.text_message_with_prefix_and_newline_and_nul_ =
      data_->streambuf_.Finalize();
  data_->entry.prefix_len_ = data_->streambuf_.prefix_len();
  SendToLog();
}

void LogMessage::SetFailQuietly() { data_->fail_quietly = true; }

bool LogMessage::IsFatal() const {
  return data_->entry.log_severity() == absl::LogSeverity::kFatal &&
         absl::log_internal::ExitOnDFatal();
}

void LogMessage::PrepareToDie() {
  // If we log a FATAL message, flush all the log destinations, then toss
  // a signal for others to catch. We leave the logs in a state that
  // someone else can use them (as long as they flush afterwards)
  if (data_->first_fatal) {
    // Notify observers about the upcoming fatal error.
    ABSL_INTERNAL_C_SYMBOL(AbslInternalOnFatalLogMessage)(data_->entry);
  }

  if (!data_->fail_quietly) {
    // Log the message first before we start collecting stack trace.
    log_internal::LogToSinks(data_->entry, absl::MakeSpan(data_->extra_sinks),
                             data_->extra_sinks_only);

    // `DumpStackTrace` generates an empty string under MSVC.
    // Adding the constant prefix here simplifies testing.
    data_->entry.stacktrace_ = "*** Check failure stack trace: ***\n";
    debugging_internal::DumpStackTrace(
        0, log_internal::MaxFramesInLogStackTrace(),
        log_internal::ShouldSymbolizeLogStackTrace(), WriteToString,
        &data_->entry.stacktrace_);
  }
}

void LogMessage::Die() {
  absl::FlushLogSinks();

  if (data_->fail_quietly) {
    FailQuietly();
  } else {
    FailWithoutStackTrace();
  }
}

void LogMessage::SendToLog() {
  if (IsFatal()) PrepareToDie();
  // Also log to all registered sinks, even if OnlyLogToStderr() is set.
  log_internal::LogToSinks(data_->entry, absl::MakeSpan(data_->extra_sinks),
                           data_->extra_sinks_only);
  if (IsFatal()) Die();
}

void LogMessage::LogBacktraceIfNeeded() {
  if (!absl::log_internal::IsInitialized()) return;

  if (!absl::log_internal::ShouldLogBacktraceAt(data_->entry.source_basename(),
                                                data_->entry.source_line()))
    return;
  stream_ << " (stacktrace:\n";
  debugging_internal::DumpStackTrace(
      1, log_internal::MaxFramesInLogStackTrace(),
      log_internal::ShouldSymbolizeLogStackTrace(), WriteToStream, &stream_);
  stream_ << ") ";
}

LogMessageFatal::LogMessageFatal(const char* file, int line)
    : LogMessage(file, line, absl::LogSeverity::kFatal) {}

LogMessageFatal::LogMessageFatal(const char* file, int line,
                                 absl::string_view failure_msg)
    : LogMessage(file, line, absl::LogSeverity::kFatal) {
  *this << "Check failed: " << failure_msg << " ";
}

// ABSL_ATTRIBUTE_NORETURN doesn't seem to work on destructors with msvc, so
// disable msvc's warning about the d'tor never returning.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4722)
#endif
LogMessageFatal::~LogMessageFatal() {
  Flush();
  FailWithoutStackTrace();
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

LogMessageQuietlyFatal::LogMessageQuietlyFatal(const char* file, int line)
    : LogMessage(file, line, absl::LogSeverity::kFatal) {
  SetFailQuietly();
}

LogMessageQuietlyFatal::LogMessageQuietlyFatal(const char* file, int line,
                                               absl::string_view failure_msg)
    : LogMessage(file, line, absl::LogSeverity::kFatal) {
  SetFailQuietly();
  *this << "Check failed: " << failure_msg << " ";
}

// ABSL_ATTRIBUTE_NORETURN doesn't seem to work on destructors with msvc, so
// disable msvc's warning about the d'tor never returning.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4722)
#endif
LogMessageQuietlyFatal::~LogMessageQuietlyFatal() {
  Flush();
  FailQuietly();
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace log_internal

ABSL_NAMESPACE_END
}  // namespace absl
