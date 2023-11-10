// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/native_messaging/pipe_messaging_channel.h"

#include <utility>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/process/process_info.h"
#include "base/values.h"
#include "build/build_config.h"

namespace remoting {

PipeMessagingChannel::PipeMessagingChannel(base::File input, base::File output)
    : native_messaging_reader_(input.Duplicate()),
      native_messaging_writer_(new NativeMessagingWriter(output.Duplicate())),
      event_handler_(nullptr) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

PipeMessagingChannel::~PipeMessagingChannel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
void PipeMessagingChannel::ReopenStdinStdout() {
#if BUILDFLAG(IS_POSIX)
  base::FilePath dev_null("/dev/null");
  int new_stdin =
      base::File(dev_null, base::File::FLAG_OPEN | base::File::FLAG_READ)
          .TakePlatformFile();
  DCHECK_EQ(new_stdin, STDIN_FILENO);
  int new_stdout =
      base::File(dev_null, base::File::FLAG_OPEN | base::File::FLAG_WRITE)
          .TakePlatformFile();
  DCHECK_EQ(new_stdout, STDOUT_FILENO);
#endif  // BUILDFLAG(IS_POSIX)
}

void PipeMessagingChannel::Start(EventHandler* event_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!event_handler_);

  event_handler_ = event_handler;
  DCHECK(event_handler_);

  native_messaging_reader_.Start(
      base::BindRepeating(&PipeMessagingChannel::ProcessMessage, weak_ptr_),
      base::BindOnce(&PipeMessagingChannel::Shutdown, weak_ptr_));
}

void PipeMessagingChannel::ProcessMessage(base::Value message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (event_handler_) {
    event_handler_->OnMessage(std::move(message));
  }
}

void PipeMessagingChannel::SendMessage(std::optional<base::ValueView> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool success = message && native_messaging_writer_;
  if (success) {
    success = native_messaging_writer_->WriteMessage(*message);
  }

  if (!success) {
    // Close the write pipe so no more responses will be sent.
    native_messaging_writer_.reset();
    Shutdown();
  }
}

void PipeMessagingChannel::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (event_handler_) {
    // Set |event_handler_| to nullptr to indicate the object is in a shutdown
    // cycle. Since event_handler->OnDisconnect() will destroy the current
    // object, |event_handler_| will become a dangling pointer after
    // OnDisconnect() returns. Therefore, we set |event_handler_| to nullptr
    // beforehand.
    EventHandler* handler = event_handler_;
    event_handler_ = nullptr;
    handler->OnDisconnect();
  }
}

}  // namespace remoting
