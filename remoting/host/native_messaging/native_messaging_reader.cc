// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/native_messaging/native_messaging_reader.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>

#include "base/threading/platform_thread.h"
#include "base/win/scoped_handle.h"
#endif  // defined(OS_WIN)

namespace {

// uint32_t is specified in the protocol as the type for the message header.
typedef uint32_t MessageLengthType;

const int kMessageHeaderSize = sizeof(MessageLengthType);

// Limit the size of received messages, to avoid excessive memory-allocation in
// this process, and potential overflow issues when casting to a signed 32-bit
// int.
const MessageLengthType kMaximumMessageSize = 1024 * 1024;

}  // namespace

namespace remoting {

class NativeMessagingReader::Core {
 public:
  Core(base::File file,
       scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
       scoped_refptr<base::SequencedTaskRunner> read_task_runner,
       base::WeakPtr<NativeMessagingReader> reader_);
  ~Core();

  // Reads a message from the Native Messaging client and passes it to
  // |message_callback_| on the originating thread. Called on the reader thread.
  void ReadMessage();

 private:
  // Notify the reader's EOF callback when an error occurs or EOF is reached.
  void NotifyEof();

  base::File read_stream_;

  base::WeakPtr<NativeMessagingReader> reader_;

  // Used to post the caller-supplied reader callbacks on the caller thread.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Used to DCHECK that the reader code executes on the correct thread.
  scoped_refptr<base::SequencedTaskRunner> read_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

NativeMessagingReader::Core::Core(
    base::File file,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SequencedTaskRunner> read_task_runner,
    base::WeakPtr<NativeMessagingReader> reader)
    : read_stream_(std::move(file)),
      reader_(reader),
      caller_task_runner_(caller_task_runner),
      read_task_runner_(read_task_runner) {
}

NativeMessagingReader::Core::~Core() = default;

void NativeMessagingReader::Core::ReadMessage() {
  DCHECK(read_task_runner_->RunsTasksInCurrentSequence());

  // Keep reading messages until the stream is closed or an error occurs.
  while (true) {
    MessageLengthType message_length;
    int read_result = read_stream_.ReadAtCurrentPos(
        reinterpret_cast<char*>(&message_length), kMessageHeaderSize);
    if (read_result != kMessageHeaderSize) {
      // 0 means EOF which is normal and should not be logged as an error.
      if (read_result != 0) {
        LOG(ERROR) << "Failed to read message header, read returned "
                   << read_result;
      }
      NotifyEof();
      return;
    }

    if (message_length > kMaximumMessageSize) {
      LOG(ERROR) << "Message size too large: " << message_length;
      NotifyEof();
      return;
    }

    std::string message_json(message_length, '\0');
    read_result =
        read_stream_.ReadAtCurrentPos(base::data(message_json), message_length);
    if (read_result != static_cast<int>(message_length)) {
      LOG(ERROR) << "Failed to read message body, read returned "
                 << read_result;
      NotifyEof();
      return;
    }

    std::unique_ptr<base::Value> message =
        base::JSONReader::ReadDeprecated(message_json);
    if (!message) {
      LOG(ERROR) << "Failed to parse JSON message: " << message.get();
      NotifyEof();
      return;
    }

    // Notify callback of new message.
    caller_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&NativeMessagingReader::InvokeMessageCallback,
                                  reader_, std::move(message)));
  }
}

void NativeMessagingReader::Core::NotifyEof() {
  DCHECK(read_task_runner_->RunsTasksInCurrentSequence());
  caller_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeMessagingReader::InvokeEofCallback, reader_));
}

NativeMessagingReader::NativeMessagingReader(base::File file)
    : reader_thread_("Reader") {
  reader_thread_.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, /*size=*/0));

  read_task_runner_ = reader_thread_.task_runner();
  core_.reset(new Core(std::move(file), base::ThreadTaskRunnerHandle::Get(),
                       read_task_runner_, weak_factory_.GetWeakPtr()));
}

NativeMessagingReader::~NativeMessagingReader() {
  read_task_runner_->DeleteSoon(FROM_HERE, core_.release());

#if defined(OS_WIN)
  // The ReadMessage() method uses a blocking read (on all platforms) which
  // cause a deadlock if the owning thread attempts to destroy this object
  // while there is a read operation pending.
  // On POSIX platforms, closing the write end of the pipe causes the Chrome
  // process to close the read end so that this class can be cleaned up.
  // On Windows, closing the write end of the pipe does nothing as the parent
  // process is cmd.exe which doesn't care.  Thus, the read end of the pipe
  // remains open, the read operation is blocked, and we hang in the d'tor.
  // Canceling the pending I/O here prevents the hang on Windows and isn't
  // needed for POSIX since it works correctly.
  base::PlatformThreadId thread_id = reader_thread_.GetThreadId();
  base::win::ScopedHandle thread_handle(
      OpenThread(THREAD_TERMINATE, /*bInheritHandle=*/false, thread_id));
  if (!CancelSynchronousIo(thread_handle.Get())) {
    // ERROR_NOT_FOUND means there were no pending IO requests so don't treat
    // that result as an error.
    if (GetLastError() != ERROR_NOT_FOUND) {
      PLOG(ERROR) << "CancelSynchronousIo() failed";
    }
  }
#endif  // defined(OS_WIN)
}

void NativeMessagingReader::Start(MessageCallback message_callback,
                                  base::Closure eof_callback) {
  message_callback_ = message_callback;
  eof_callback_ = eof_callback;

  // base::Unretained is safe since |core_| is only deleted via the
  // DeleteSoon task which is posted from this class's dtor.
  read_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NativeMessagingReader::Core::ReadMessage,
                                base::Unretained(core_.get())));
}

void NativeMessagingReader::InvokeMessageCallback(
    std::unique_ptr<base::Value> message) {
  message_callback_.Run(std::move(message));
}

void NativeMessagingReader::InvokeEofCallback() {
  eof_callback_.Run();
}

}  // namespace remoting
