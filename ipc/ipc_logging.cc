// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_logging.h"

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
#define IPC_MESSAGE_MACROS_LOG_ENABLED
#endif

#include <stddef.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_sync_message.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>
#endif

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)

using base::Time;

namespace IPC {

const int kLogSendDelayMs = 100;

// We use a pointer to the function table to avoid any linker dependencies on
// all the traits used as IPC message parameters.
LogFunctionMap* Logging::log_function_map_;

Logging::Logging()
    : enabled_(false),
      enabled_on_stderr_(false),
      enabled_color_(false),
      queue_invoke_later_pending_(false),
      sender_(nullptr),
      main_thread_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      consumer_(nullptr) {
#if BUILDFLAG(IS_WIN)
  // getenv triggers an unsafe warning. Simply check how big of a buffer
  // would be needed to fetch the value to see if the enviornment variable is
  // set.
  size_t requiredSize = 0;
  getenv_s(&requiredSize, NULL, 0, "CHROME_IPC_LOGGING");
  bool logging_env_var_set = (requiredSize != 0);
  if (requiredSize <= 6) {
    char buffer[6];
    getenv_s(&requiredSize, buffer, sizeof(buffer), "CHROME_IPC_LOGGING");
    if (requiredSize && !strncmp("color", buffer, 6))
      enabled_color_ = true;
  }
#else   // !BUILDFLAG(IS_WIN)
  const char* ipc_logging = getenv("CHROME_IPC_LOGGING");
  bool logging_env_var_set = (ipc_logging != NULL);
  if (ipc_logging && !strcmp(ipc_logging, "color"))
    enabled_color_ = true;
#endif  // BUILDFLAG(IS_WIN)
  if (logging_env_var_set) {
    enabled_ = true;
    enabled_on_stderr_ = true;
  }
}

Logging::~Logging() {
}

Logging* Logging::GetInstance() {
  return base::Singleton<Logging>::get();
}

void Logging::SetConsumer(Consumer* consumer) {
  consumer_ = consumer;
}

void Logging::Enable() {
  enabled_ = true;
}

void Logging::Disable() {
  enabled_ = false;
}

void Logging::OnSendLogs() {
  queue_invoke_later_pending_ = false;
  if (!sender_)
    return;

  Message* msg = new Message(
      MSG_ROUTING_CONTROL, IPC_LOGGING_ID, Message::PRIORITY_NORMAL);
  WriteParam(msg, queued_logs_);
  queued_logs_.clear();
  sender_->Send(msg);
}

void Logging::SetIPCSender(IPC::Sender* sender) {
  sender_ = sender;
}

void Logging::OnReceivedLoggingMessage(const Message& message) {
  std::vector<LogData> data;
  base::PickleIterator iter(message);
  if (!ReadParam(&message, &iter, &data))
    return;

  for (size_t i = 0; i < data.size(); ++i) {
    Log(data[i]);
  }
}

void Logging::OnSendMessage(Message* message) {
  if (!Enabled())
    return;

  if (message->is_reply()) {
    LogData* data = message->sync_log_data();
    if (!data)
      return;

    // This is actually the delayed reply to a sync message.  Create a string
    // of the output parameters, add it to the LogData that was earlier stashed
    // with the reply, and log the result.
    GenerateLogData(*message, data, true);
    Log(*data);
    delete data;
    message->set_sync_log_data(NULL);
  } else {
    // If the time has already been set (i.e. by ChannelProxy), keep that time
    // instead as it's more accurate.
    if (!message->sent_time()) {
      message->set_sent_time(
          Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
    }
  }
}

void Logging::OnPreDispatchMessage(const Message& message) {
  message.set_received_time(
      Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
}

void Logging::OnPostDispatchMessage(const Message& message) {
  if (!Enabled() ||
      !message.sent_time() ||
      !message.received_time() ||
      message.dont_log())
    return;

  LogData data;
  GenerateLogData(message, &data, true);

  if (main_thread_->BelongsToCurrentThread()) {
    Log(data);
  } else {
    main_thread_->PostTask(
        FROM_HERE, base::BindOnce(&Logging::Log, base::Unretained(this), data));
  }
}

void Logging::GetMessageText(uint32_t type, std::string* name,
                             const Message* message,
                             std::string* params) {
  if (!log_function_map_)
    return;

  LogFunctionMap::iterator it = log_function_map_->find(type);
  if (it == log_function_map_->end()) {
    if (name) {
      *name = "[UNKNOWN MSG ";
      *name += base::NumberToString(type);
      *name += " ]";
    }
    return;
  }

  (*it->second)(name, message, params);
}

const char* Logging::ANSIEscape(ANSIColor color) {
  if (!enabled_color_)
    return "";
  switch (color) {
    case ANSI_COLOR_RESET:
      return "\033[m";
    case ANSI_COLOR_BLACK:
      return "\033[0;30m";
    case ANSI_COLOR_RED:
      return "\033[0;31m";
    case ANSI_COLOR_GREEN:
      return "\033[0;32m";
    case ANSI_COLOR_YELLOW:
      return "\033[0;33m";
    case ANSI_COLOR_BLUE:
      return "\033[0;34m";
    case ANSI_COLOR_MAGENTA:
      return "\033[0;35m";
    case ANSI_COLOR_CYAN:
      return "\033[0;36m";
    case ANSI_COLOR_WHITE:
      return "\033[0;37m";
  }
  return "";
}

Logging::ANSIColor Logging::DelayColor(double delay) {
  if (delay < 0.1)
    return ANSI_COLOR_GREEN;
  if (delay < 0.25)
    return ANSI_COLOR_BLACK;
  if (delay < 0.5)
    return ANSI_COLOR_YELLOW;
  return ANSI_COLOR_RED;
}

void Logging::Log(const LogData& data) {
  if (consumer_) {
    // We're in the browser process.
    consumer_->Log(data);
  } else {
    // We're in the renderer or plugin processes.
    if (sender_) {
      queued_logs_.push_back(data);
      if (!queue_invoke_later_pending_) {
        queue_invoke_later_pending_ = true;
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&Logging::OnSendLogs, base::Unretained(this)),
            base::Milliseconds(kLogSendDelayMs));
      }
    }
  }
  if (enabled_on_stderr_) {
    std::string message_name;
    if (data.message_name.empty()) {
      message_name = base::StringPrintf("[unknown type %d]", data.type);
    } else {
      message_name = data.message_name;
    }
    double receive_delay =
        (Time::FromDeltaSinceWindowsEpoch(base::Microseconds(data.receive)) -
         Time::FromDeltaSinceWindowsEpoch(base::Microseconds(data.sent)))
            .InSecondsF();
    double dispatch_delay =
        (Time::FromDeltaSinceWindowsEpoch(base::Microseconds(data.dispatch)) -
         Time::FromDeltaSinceWindowsEpoch(base::Microseconds(data.sent)))
            .InSecondsF();
    fprintf(stderr, "ipc %d %s %s%s %s%s\n  %18.5f %s%18.5f %s%18.5f%s\n",
            data.routing_id, data.flags.c_str(),
            ANSIEscape(sender_ ? ANSI_COLOR_BLUE : ANSI_COLOR_CYAN),
            message_name.c_str(), ANSIEscape(ANSI_COLOR_RESET),
            data.params.c_str(),
            Time::FromDeltaSinceWindowsEpoch(base::Microseconds(data.sent))
                .InSecondsFSinceUnixEpoch(),
            ANSIEscape(DelayColor(receive_delay)),
            Time::FromDeltaSinceWindowsEpoch(base::Microseconds(data.receive))
                .InSecondsFSinceUnixEpoch(),
            ANSIEscape(DelayColor(dispatch_delay)),
            Time::FromDeltaSinceWindowsEpoch(base::Microseconds(data.dispatch))
                .InSecondsFSinceUnixEpoch(),
            ANSIEscape(ANSI_COLOR_RESET));
  }
}

void GenerateLogData(const Message& message, LogData* data, bool get_params) {
  if (message.is_reply()) {
    // "data" should already be filled in.
    std::string params;
    Logging::GetMessageText(data->type, NULL, &message, &params);

    if (!data->params.empty() && !params.empty())
      data->params += ", ";

    data->flags += " DR";

    data->params += params;
  } else {
    std::string flags;
    if (message.is_sync())
      flags = "S";

    if (message.is_reply())
      flags += "R";

    if (message.is_reply_error())
      flags += "E";

    std::string params, message_name;
    Logging::GetMessageText(message.type(), &message_name, &message,
                            get_params ? &params : NULL);

    data->routing_id = message.routing_id();
    data->type = message.type();
    data->flags = flags;
    data->sent = message.sent_time();
    data->receive = message.received_time();
    data->dispatch = Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
    data->params = params;
    data->message_name = message_name;
  }
}

}

#endif  // BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
