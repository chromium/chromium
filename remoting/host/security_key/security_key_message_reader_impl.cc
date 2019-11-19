// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_message_reader_impl.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/host/security_key/security_key_message.h"

namespace remoting {

SecurityKeyMessageReaderImpl::SecurityKeyMessageReaderImpl(
    base::File input_file)
    : read_stream_(std::move(input_file)),
      reader_thread_("SecurityKeyMessageReaderImpl") {
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  reader_thread_.StartWithOptions(options);

  read_task_runner_ = reader_thread_.task_runner();
  main_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

SecurityKeyMessageReaderImpl::~SecurityKeyMessageReaderImpl() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  // In order to ensure the reader thread is stopped cleanly, we want to stop
  // the thread before the task runners and weak pointers are invalidated.
  reader_thread_.Stop();
}

void SecurityKeyMessageReaderImpl::Start(
    const SecurityKeyMessageCallback& message_callback,
    const base::Closure& error_callback) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  message_callback_ = message_callback;
  error_callback_ = error_callback;

  // base::Unretained is safe since this class owns the thread running this task
  // which will be destroyed before this instance is.
  read_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SecurityKeyMessageReaderImpl::ReadMessage,
                                base::Unretained(this)));
}

void SecurityKeyMessageReaderImpl::ReadMessage() {
  DCHECK(read_task_runner_->RunsTasksInCurrentSequence());

  while (true) {
    if (!read_stream_.IsValid()) {
      LOG(ERROR) << "Cannot read from invalid stream.";
      NotifyError();
      return;
    }

    uint32_t message_length_bytes = 0;
    if (!ReadFromStream(reinterpret_cast<char*>(&message_length_bytes), 4)) {
      NotifyError();
      return;
    }

    if (!SecurityKeyMessage::IsValidMessageSize(message_length_bytes)) {
      LOG(ERROR) << "Message size is invalid: " << message_length_bytes;
      NotifyError();
      return;
    }

    std::string message_data(message_length_bytes, '\0');
    if (!ReadFromStream(base::data(message_data), message_data.size())) {
      NotifyError();
      return;
    }

    std::unique_ptr<SecurityKeyMessage> message(new SecurityKeyMessage());
    if (!message->ParseMessage(message_data)) {
      LOG(ERROR) << "Invalid message data received.";
      NotifyError();
      return;
    }

    // Notify callback of the new message received.
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SecurityKeyMessageReaderImpl::InvokeMessageCallback,
                       weak_factory_.GetWeakPtr(), std::move(message)));
  }
}

bool SecurityKeyMessageReaderImpl::ReadFromStream(char* buffer,
                                                  size_t bytes_to_read) {
  DCHECK(buffer);
  DCHECK_GT(bytes_to_read, 0u);

  size_t bytes_read = 0;
  do {
    int read_result = read_stream_.ReadAtCurrentPosNoBestEffort(
        buffer + bytes_read, bytes_to_read - bytes_read);
    if (read_result < 1) {
      // 0 means EOF which is normal and should not be logged as an error.
      if (read_result != 0) {
        LOG(ERROR) << "Failed to read from stream, ReadAtCurrentPos returned "
                   << read_result;
      }
      return false;
    }
    bytes_read += read_result;
  } while (bytes_read < bytes_to_read);
  DCHECK_EQ(bytes_read, bytes_to_read);

  return true;
}

void SecurityKeyMessageReaderImpl::NotifyError() {
  DCHECK(read_task_runner_->RunsTasksInCurrentSequence());

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SecurityKeyMessageReaderImpl::InvokeErrorCallback,
                     weak_factory_.GetWeakPtr()));
}

void SecurityKeyMessageReaderImpl::InvokeMessageCallback(
    std::unique_ptr<SecurityKeyMessage> message) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  message_callback_.Run(std::move(message));
}

void SecurityKeyMessageReaderImpl::InvokeErrorCallback() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  error_callback_.Run();
}

}  // namespace remoting
