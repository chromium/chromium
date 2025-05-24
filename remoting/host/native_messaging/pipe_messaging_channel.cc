// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/native_messaging/pipe_messaging_channel.h"

#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/process/process_info.h"
#include "base/values.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#endif

namespace remoting {

namespace {

#if BUILDFLAG(IS_POSIX)
// Takes ownership of `fd`, returns a duplicate of `fd`, then points `fd` to
// /dev/null.
base::File DuplicateAndBlock(int fd, base::File::Flags read_write_flag) {
  base::File original{fd};
  base::File target_dup = original.Duplicate();
  // base::File closes the file descriptor in its destructor, which is an
  // unwanted side effect, so we call TakePlatformFile() to release the FD.
  int original_fd = original.TakePlatformFile();
  CHECK_EQ(original_fd, fd);
  base::File dev_null = base::File(base::FilePath("/dev/null"),
                                   base::File::FLAG_OPEN | read_write_flag);
  // dup2() closes `newfd` and duplicates `oldfd` into `newfd` at the same time,
  // preventing race conditions, whereby newfd might be reused between close()
  // and dup() on another thread.
  int new_fd = HANDLE_EINTR(dup2(dev_null.GetPlatformFile(), fd));
  PCHECK(new_fd != -1) << "Unexpected error when executing dup2 with fd " << fd;
  DCHECK_EQ(new_fd, fd);
  return target_dup;
}
#endif

}  // namespace

PipeMessagingChannel::PipeMessagingChannel(base::File input, base::File output)
    : native_messaging_reader_(std::move(input)),
      native_messaging_writer_(new NativeMessagingWriter(std::move(output))),
      event_handler_(nullptr) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

PipeMessagingChannel::~PipeMessagingChannel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

#if BUILDFLAG(IS_POSIX)
// static
void PipeMessagingChannel::OpenAndBlockStdio(base::File& stdin_file,
                                             base::File& stdout_file) {
  stdin_file = DuplicateAndBlock(STDIN_FILENO, base::File::FLAG_READ);
  stdout_file = DuplicateAndBlock(STDOUT_FILENO, base::File::FLAG_WRITE);
}
#endif  // BUILDFLAG(IS_POSIX)

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
