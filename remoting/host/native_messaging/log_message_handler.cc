// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/native_messaging/log_message_handler.h"

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"

namespace remoting {

namespace {
// Log message handler registration and deregistration is not thread-safe.
// This lock must be held in order to set or restore a log message handler,
// and when accessing the pointer to the LogMessageHandler instance.
base::LazyInstance<base::Lock>::Leaky g_log_message_handler_lock =
    LAZY_INSTANCE_INITIALIZER;

// The singleton LogMessageHandler instance, or null if log messages are not
// being intercepted. It must be dereferenced and updated under the above lock.
LogMessageHandler* g_log_message_handler = nullptr;
}  // namespace

LogMessageHandler::LogMessageHandler(const Delegate& delegate)
    : delegate_(delegate),
      suppress_logging_(false),
      caller_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  base::AutoLock lock(g_log_message_handler_lock.Get());
  if (g_log_message_handler) {
    LOG(FATAL) << "LogMessageHandler is already registered. Only one instance "
               << "per process is allowed.";
  }
  previous_log_message_handler_ = logging::GetLogMessageHandler();
  logging::SetLogMessageHandler(&LogMessageHandler::OnLogMessage);
  g_log_message_handler = this;
}

LogMessageHandler::~LogMessageHandler() {
  base::AutoLock lock(g_log_message_handler_lock.Get());
  if (logging::GetLogMessageHandler() != &LogMessageHandler::OnLogMessage) {
    LOG(FATAL) << "LogMessageHandler is not the top-most message handler. "
               << "Cannot unregister.";
  }
  logging::SetLogMessageHandler(previous_log_message_handler_);
  g_log_message_handler = nullptr;
}

// static
const char* LogMessageHandler::kDebugMessageTypeName = "_debug_log";

// static
bool LogMessageHandler::OnLogMessage(logging::LogSeverity severity,
                                     const char* file,
                                     int line,
                                     size_t message_start,
                                     const std::string& str) {
  base::AutoLock lock(g_log_message_handler_lock.Get());
  if (g_log_message_handler) {
    g_log_message_handler->PostLogMessageToCorrectThread(severity, file, line,
                                                         message_start, str);
  }
  return false;
}

void LogMessageHandler::PostLogMessageToCorrectThread(
    logging::LogSeverity severity,
    const char* file,
    int line,
    size_t message_start,
    const std::string& str) {
  if (caller_task_runner_->BelongsToCurrentThread()) {
    // Don't process this message if we're already logging and on the caller
    // thread. This guards against an infinite loop if any code called by this
    // class logs something.
    if (suppress_logging_) {
      return;
    }

    if (log_synchronously_if_possible_) {
      SendLogMessageToClient(severity, file, line, message_start, str);
      return;
    }
  }

  // This method is always called under the global lock, so post a task to
  // handle the log message, even if we're already on the correct thread, unless
  // it is explicitly configured not to do so. This allows the lock to be
  // released quickly.
  //
  // Note that this means that LOG(FATAL) messages will be lost because the
  // process will exit before the message is sent to the client.
  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LogMessageHandler::SendLogMessageToClient,
                                weak_ptr_factory_.GetWeakPtr(), severity, file,
                                line, message_start, str));
}

void LogMessageHandler::SendLogMessageToClient(logging::LogSeverity severity,
                                               const char* file,
                                               int line,
                                               size_t message_start,
                                               const std::string& str) {
  suppress_logging_ = true;

  std::string severity_string = "log";
  switch (severity) {
    case logging::LOGGING_WARNING:
      severity_string = "warn";
      break;
    case logging::LOGGING_ERROR:
    case logging::LOGGING_FATAL:
      severity_string = "error";
      break;
  }

  std::string message = str.substr(message_start);
  base::TrimWhitespaceASCII(message, base::TRIM_ALL, &message);

  base::Value::Dict dictionary;
  dictionary.Set("type", kDebugMessageTypeName);
  dictionary.Set("severity", severity_string);
  dictionary.Set("message", message);
  dictionary.Set("file", file);
  dictionary.Set("line", line);

  // Protect against this instance being torn down after the delegate is run.
  base::WeakPtr<LogMessageHandler> self = weak_ptr_factory_.GetWeakPtr();
  delegate_.Run(std::move(dictionary));

  if (self) {
    suppress_logging_ = false;
  }
}

}  // namespace remoting
