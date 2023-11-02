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
// File: log/internal/log_message.h
// -----------------------------------------------------------------------------
//
// This file declares `class absl::log_internal::LogMessage`. This class more or
// less represents a particular log message. LOG/CHECK macros create a
// temporary instance of `LogMessage` and then stream values to it.  At the end
// of the LOG/CHECK statement, LogMessage instance goes out of scope and
// `~LogMessage` directs the message to the registered log sinks.
// Heap-allocation of `LogMessage` is unsupported.  Construction outside of a
// `LOG` macro is unsupported.

#ifndef ABSL_LOG_INTERNAL_LOG_MESSAGE_H_
#define ABSL_LOG_INTERNAL_LOG_MESSAGE_H_

#include <ios>
#include <memory>
#include <ostream>
#include <streambuf>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/internal/errno_saver.h"
#include "absl/base/log_severity.h"
#include "absl/log/internal/config.h"
#include "absl/log/internal/nullguard.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace log_internal {

constexpr int kLogMessageBufferSize = 15000;

class LogMessage {
 public:
  // Used for `LOG`.
  LogMessage(const char* file, int line,
             absl::LogSeverity severity) ABSL_ATTRIBUTE_COLD;
  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;
  ~LogMessage() ABSL_ATTRIBUTE_COLD;

  // Overrides the location inferred from the callsite.  The string pointed to
  // by `file` must be valid until the end of the statement.
  LogMessage& AtLocation(absl::string_view file, int line);
  // Omits the prefix from this line.  The prefix includes metadata about the
  // logged data such as source code location and timestamp.
  LogMessage& NoPrefix();
  // Sets the verbosity field of the logged message as if it was logged by
  // `VLOG(verbose_level)`.  Unlike `VLOG`, this method does not affect
  // evaluation of the statement when the specified `verbose_level` has been
  // disabled.  The only effect is on `absl::LogSink` implementations which
  // make use of the `absl::LogSink::verbosity()` value.  The value
  // `absl::LogEntry::kNoVerbosityLevel` can be specified to mark the message
  // not verbose.
  LogMessage& WithVerbosity(int verbose_level);
  // Uses the specified timestamp instead of one collected in the constructor.
  LogMessage& WithTimestamp(absl::Time timestamp);
  // Uses the specified thread ID instead of one collected in the constructor.
  LogMessage& WithThreadID(absl::LogEntry::tid_t tid);
  // Copies all metadata (but no data) from the specified `absl::LogEntry`.
  LogMessage& WithMetadataFrom(const absl::LogEntry& entry);
  // Appends to the logged message a colon, a space, a textual description of
  // the current value of `errno` (as by strerror(3)), and the numerical value
  // of `errno`.
  LogMessage& WithPerror();
  // Sends this message to `*sink` in addition to whatever other sinks it would
  // otherwise have been sent to.  `sink` must not be null.
  LogMessage& ToSinkAlso(absl::LogSink* sink);
  // Sends this message to `*sink` and no others.  `sink` must not be null.
  LogMessage& ToSinkOnly(absl::LogSink* sink);

  // Don't call this method from outside this library.
  LogMessage& InternalStream() { return *this; }

  // By-value overloads for small, common types let us overlook common failures
  // to define globals and static data members (i.e. in a .cc file).
  // clang-format off
  // The CUDA toolchain cannot handle these <<<'s:
  LogMessage& operator<<(char v) { return operator<< <char>(v); }
  LogMessage& operator<<(signed char v) { return operator<< <signed char>(v); }
  LogMessage& operator<<(unsigned char v) {
    return operator<< <unsigned char>(v);
  }
  LogMessage& operator<<(signed short v) {  // NOLINT
    return operator<< <signed short>(v);  // NOLINT
  }
  LogMessage& operator<<(signed int v) { return operator<< <signed int>(v); }
  LogMessage& operator<<(signed long v) {  // NOLINT
    return operator<< <signed long>(v);  // NOLINT
  }
  LogMessage& operator<<(signed long long v) {  // NOLINT
    return operator<< <signed long long>(v);  // NOLINT
  }
  LogMessage& operator<<(unsigned short v) {  // NOLINT
    return operator<< <unsigned short>(v);  // NOLINT
  }
  LogMessage& operator<<(unsigned int v) {
    return operator<< <unsigned int>(v);
  }
  LogMessage& operator<<(unsigned long v) {  // NOLINT
    return operator<< <unsigned long>(v);  // NOLINT
  }
  LogMessage& operator<<(unsigned long long v) {  // NOLINT
    return operator<< <unsigned long long>(v);  // NOLINT
  }
  LogMessage& operator<<(void* v) { return operator<< <void*>(v); }
  LogMessage& operator<<(const void* v) { return operator<< <const void*>(v); }
  LogMessage& operator<<(float v) { return operator<< <float>(v); }
  LogMessage& operator<<(double v) { return operator<< <double>(v); }
  LogMessage& operator<<(bool v) { return operator<< <bool>(v); }
  // clang-format on

  // Handle stream manipulators e.g. std::endl.
  LogMessage& operator<<(std::ostream& (*m)(std::ostream& os));
  LogMessage& operator<<(std::ios_base& (*m)(std::ios_base& os));

  // Literal strings.  This allows us to record C string literals as literals in
  // the logging.proto.Value.
  //
  // Allow this overload to be inlined to prevent generating instantiations of
  // this template for every value of `SIZE` encountered in each source code
  // file. That significantly increases linker input sizes. Inlining is cheap
  // because the argument to this overload is almost always a string literal so
  // the call to `strlen` can be replaced at compile time. The overload for
  // `char[]` below should not be inlined. The compiler typically does not have
  // the string at compile time and cannot replace the call to `strlen` so
  // inlining it increases the binary size. See the discussion on
  // cl/107527369.
  template <int SIZE>
  LogMessage& operator<<(const char (&buf)[SIZE]);

  // This prevents non-const `char[]` arrays from looking like literals.
  template <int SIZE>
  LogMessage& operator<<(char (&buf)[SIZE]) ABSL_ATTRIBUTE_NOINLINE;

  // Default: uses `ostream` logging to convert `v` to a string.
  template <typename T>
  LogMessage& operator<<(const T& v) ABSL_ATTRIBUTE_NOINLINE;

  // Note: We explicitly do not support `operator<<` for non-const references
  // because it breaks logging of non-integer bitfield types (i.e., enums).

 protected:
  // Call `abort()` or similar to perform `LOG(FATAL)` crash.  It is assumed
  // that the caller has already generated and written the trace as appropriate.
  ABSL_ATTRIBUTE_NORETURN static void FailWithoutStackTrace();

  // Similar to `FailWithoutStackTrace()`, but without `abort()`.  Terminates
  // the process with an error exit code.
  ABSL_ATTRIBUTE_NORETURN static void FailQuietly();

  // Dispatches the completed `absl::LogEntry` to applicable `absl::LogSink`s.
  // This might as well be inlined into `~LogMessage` except that
  // `~LogMessageFatal` needs to call it early.
  void Flush();

  // After this is called, failures are done as quiet as possible for this log
  // message.
  void SetFailQuietly();

 private:
  struct LogMessageData;  // Opaque type containing message state

  // Returns `true` if the message is fatal or enabled debug-fatal.
  bool IsFatal() const;

  // Records some tombstone-type data in anticipation of `Die`.
  void PrepareToDie();
  void Die();

  void SendToLog();

  // Checks `FLAGS_log_backtrace_at` and appends a backtrace if appropriate.
  void LogBacktraceIfNeeded();

  // This should be the first data member so that its initializer captures errno
  // before any other initializers alter it (e.g. with calls to new) and so that
  // no other destructors run afterward an alter it (e.g. with calls to delete).
  absl::base_internal::ErrnoSaver errno_saver_;

  // We keep the data in a separate struct so that each instance of `LogMessage`
  // uses less stack space.
  std::unique_ptr<LogMessageData> data_;

  std::ostream stream_;
};

// Note: the following is declared `ABSL_ATTRIBUTE_NOINLINE`
template <typename T>
LogMessage& LogMessage::operator<<(const T& v) {
  stream_ << log_internal::NullGuard<T>().Guard(v);
  return *this;
}
inline LogMessage& LogMessage::operator<<(
    std::ostream& (*m)(std::ostream& os)) {
  stream_ << m;
  return *this;
}
inline LogMessage& LogMessage::operator<<(
    std::ios_base& (*m)(std::ios_base& os)) {
  stream_ << m;
  return *this;
}
template <int SIZE>
LogMessage& LogMessage::operator<<(const char (&buf)[SIZE]) {
  stream_ << buf;
  return *this;
}
// Note: the following is declared `ABSL_ATTRIBUTE_NOINLINE`
template <int SIZE>
LogMessage& LogMessage::operator<<(char (&buf)[SIZE]) {
  stream_ << buf;
  return *this;
}
// We instantiate these specializations in the library's TU to save space in
// other TUs.  Since the template is marked `ABSL_ATTRIBUTE_NOINLINE` we will be
// emitting a function call either way.
extern template LogMessage& LogMessage::operator<<(const char& v);
extern template LogMessage& LogMessage::operator<<(const signed char& v);
extern template LogMessage& LogMessage::operator<<(const unsigned char& v);
extern template LogMessage& LogMessage::operator<<(const short& v);  // NOLINT
extern template LogMessage& LogMessage::operator<<(
    const unsigned short& v);  // NOLINT
extern template LogMessage& LogMessage::operator<<(const int& v);
extern template LogMessage& LogMessage::operator<<(
    const unsigned int& v);                                         // NOLINT
extern template LogMessage& LogMessage::operator<<(const long& v);  // NOLINT
extern template LogMessage& LogMessage::operator<<(
    const unsigned long& v);  // NOLINT
extern template LogMessage& LogMessage::operator<<(
    const long long& v);  // NOLINT
extern template LogMessage& LogMessage::operator<<(
    const unsigned long long& v);  // NOLINT
extern template LogMessage& LogMessage::operator<<(void* const& v);
extern template LogMessage& LogMessage::operator<<(const void* const& v);
extern template LogMessage& LogMessage::operator<<(const float& v);
extern template LogMessage& LogMessage::operator<<(const double& v);
extern template LogMessage& LogMessage::operator<<(const bool& v);
extern template LogMessage& LogMessage::operator<<(const std::string& v);
extern template LogMessage& LogMessage::operator<<(const absl::string_view& v);

// `LogMessageFatal` ensures the process will exit in failure after logging this
// message.
class LogMessageFatal final : public LogMessage {
 public:
  LogMessageFatal(const char* file, int line) ABSL_ATTRIBUTE_COLD;
  LogMessageFatal(const char* file, int line,
                  absl::string_view failure_msg) ABSL_ATTRIBUTE_COLD;
  ABSL_ATTRIBUTE_NORETURN ~LogMessageFatal();
};

class LogMessageQuietlyFatal final : public LogMessage {
 public:
  LogMessageQuietlyFatal(const char* file, int line) ABSL_ATTRIBUTE_COLD;
  LogMessageQuietlyFatal(const char* file, int line,
                         absl::string_view failure_msg) ABSL_ATTRIBUTE_COLD;
  ABSL_ATTRIBUTE_NORETURN ~LogMessageQuietlyFatal();
};

}  // namespace log_internal
ABSL_NAMESPACE_END
}  // namespace absl

extern "C" ABSL_ATTRIBUTE_WEAK void ABSL_INTERNAL_C_SYMBOL(
    AbslInternalOnFatalLogMessage)(const absl::LogEntry&);

#endif  // ABSL_LOG_INTERNAL_LOG_MESSAGE_H_
