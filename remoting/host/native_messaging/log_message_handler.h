// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_NATIVE_MESSAGING_LOG_MESSAGE_HANDLER_H_
#define REMOTING_HOST_NATIVE_MESSAGING_LOG_MESSAGE_HANDLER_H_

#include <stddef.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/logging.h"
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
  using Delegate = base::RepeatingCallback<void(base::Value::Dict message)>;

  explicit LogMessageHandler(const Delegate& delegate);
  ~LogMessageHandler();

  // When set to true, if a message is logged on the caller thread, the message
  // will be synchronously sent to the delegate; otherwise a task will always
  // be posted to the caller thread to handle the message. Defaults to false to
  // prevent blocking LOG calls. Set this to false if you want to make sure a
  // log gets handled when the caller sequence is about to be terminated.
  void set_log_synchronously_if_possible(bool log_synchronously_if_possible) {
    log_synchronously_if_possible_ = log_synchronously_if_possible;
  }

  static const char* kDebugMessageTypeName;

 private:
  // TODO(yuweih): Reimplement this class using using a message queue which is
  // protected by |g_log_message_handler_lock|.
  static bool OnLogMessage(int severity,
                           const char* file,
                           int line,
                           size_t message_start,
                           const std::string& str);
  void PostLogMessageToCorrectThread(int severity,
                                     const char* file,
                                     int line,
                                     size_t message_start,
                                     const std::string& str);
  void SendLogMessageToClient(int severity,
                              const char* file,
                              int line,
                              size_t message_start,
                              const std::string& str);

  Delegate delegate_;
  bool suppress_logging_;
  bool log_synchronously_if_possible_ = false;
  // TODO(yuweih): Replace all "thread" references in this class with
  // "sequence".
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  logging::LogMessageHandlerFunction previous_log_message_handler_;
  base::WeakPtrFactory<LogMessageHandler> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_NATIVE_MESSAGING_LOG_MESSAGE_HANDLER_H_
