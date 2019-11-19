// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_NATIVE_MESSAGIN_LOG_HANDLER_H_
#define REMOTING_HOST_NATIVE_MESSAGIN_LOG_HANDLER_H_

#include <stddef.h>

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace remoting {

// Helper class for logging::SetLogMessageHandler to deliver log messages to
// a consistent thread in a thread-safe way and in a format suitable for sending
// over a Native Messaging channel.
class LogMessageHandler {
 public:
  typedef base::Callback<void(std::unique_ptr<base::Value> message)> Delegate;

  explicit LogMessageHandler(const Delegate& delegate);
  ~LogMessageHandler();

  static const char* kDebugMessageTypeName;

 private:
  static bool OnLogMessage(
      int severity, const char* file, int line,
      size_t message_start, const std::string& str);
  void PostLogMessageToCorrectThread(
      int severity, const char* file, int line,
      size_t message_start, const std::string& str);
  void SendLogMessageToClient(
      int severity, const char* file, int line,
      size_t message_start, const std::string& str);

  Delegate delegate_;
  bool suppress_logging_;
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  logging::LogMessageHandlerFunction previous_log_message_handler_;
  base::WeakPtrFactory<LogMessageHandler> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif
