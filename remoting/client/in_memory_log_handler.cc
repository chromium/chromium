// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/in_memory_log_handler.h"

#include "base/check.h"
#include "base/containers/ring_buffer.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"

namespace remoting {

namespace {

constexpr size_t kMaxNumberOfLogs = 1000;

struct LogHandlerContext {
  base::Lock lock;
  base::RingBuffer<std::string, kMaxNumberOfLogs> buffer;
};

// Leaky.
LogHandlerContext* g_log_handler_context = nullptr;

bool HandleLogMessage(int severity,
                      const char* file,
                      int line,
                      size_t message_start,
                      const std::string& str) {
  base::AutoLock auto_lock(g_log_handler_context->lock);
  g_log_handler_context->buffer.SaveToBuffer(str);

  // Pass log messages through the default logging pipeline.
  return false;
}

}  // namespace

// static
void InMemoryLogHandler::Register() {
  DCHECK(!g_log_handler_context);
  DCHECK(!logging::GetLogMessageHandler())
      << "Log message handler has already been set.";

  g_log_handler_context = new LogHandlerContext();

  base::AutoLock auto_lock(g_log_handler_context->lock);
  logging::SetLogMessageHandler(&HandleLogMessage);
}

// static
std::string InMemoryLogHandler::GetInMemoryLogs() {
  std::string output;

  base::AutoLock auto_lock(g_log_handler_context->lock);

  for (auto iter = g_log_handler_context->buffer.Begin(); iter; ++iter) {
    if (iter != g_log_handler_context->buffer.Begin()) {
      output += '\n';
    }
    // *iter returns a const std::string*.
    output += **iter;
  }
  return output;
}

}  // namespace remoting
