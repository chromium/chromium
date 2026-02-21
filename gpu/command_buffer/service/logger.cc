// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/logger.h"

#include <inttypes.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "gpu/command_buffer/common/debug_marker_manager.h"
#include "gpu/command_buffer/service/gpu_switches.h"

namespace gpu {
namespace gles2 {

Logger::Logger(const DebugMarkerManager* debug_marker_manager,
               const LogMessageCallback& callback,
               bool disable_gl_error_limit)
    : debug_marker_manager_(debug_marker_manager),
      log_message_callback_(callback),
      log_message_count_(0),
      log_synthesized_gl_errors_(true),
      disable_gl_error_limit_(disable_gl_error_limit) {
  this_in_hex_ =
      base::StringPrintf("GroupMarkerNotSet(crbug.com/242999)!:%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this));
  suppress_performance_logs_ =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSuppressPerformanceLogs);
}

Logger::~Logger() = default;

void Logger::LogMessage(
    const char* filename, int line, const std::string& msg) {
  if (log_message_count_ < kMaxLogMessages || disable_gl_error_limit_) {
    std::string prefixed_msg(std::string("[") + GetLogPrefix() + "]" + msg);
    ++log_message_count_;
    // LOG this unless logging is turned off as any chromium code that
    // generates these errors probably has a bug.
    if (log_synthesized_gl_errors_) {
      ::logging::LogMessage(filename, line, ::logging::LOGGING_ERROR).stream()
          << prefixed_msg;
    }
    log_message_callback_.Run(prefixed_msg);
  } else {
    if (log_message_count_ == kMaxLogMessages) {
      ++log_message_count_;
      LOG(ERROR)
          << "Too many GL errors, not reporting any more for this context."
          << " use --disable-gl-error-limit to see all errors.";
    }
  }
}

const std::string& Logger::GetLogPrefix() const {
  const std::string& prefix(debug_marker_manager_->GetMarker());
  return prefix.empty() ? this_in_hex_ : prefix;
}

bool Logger::SuppressPerformanceLogs() const {
  return suppress_performance_logs_;
}

}  // namespace gles2
}  // namespace gpu
