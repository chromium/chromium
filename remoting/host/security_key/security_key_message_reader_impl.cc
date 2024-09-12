// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/host/security_key/security_key_message_reader_impl.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/security_key/security_key_message.h"

namespace remoting {

SecurityKeyMessageReaderImpl::SecurityKeyMessageReaderImpl(
    base::File input_file)
    : read_stream_(std::move(input_file)),
      reader_thread_("SecurityKeyMessageReaderImpl") {
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  reader_thread_.StartWithOptions(std::move(options));

  read_task_runner_ = reader_thread_.task_runner();
  main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

SecurityKeyMessageReaderImpl::~SecurityKeyMessageReaderImpl() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  // In order to ensure the reader thread is stopped cleanly, we want to stop
  // the thread before the task runners and weak pointers are invalidated.
  reader_thread_.Stop();
}

void SecurityKeyMessageReaderImpl::Start(
    const SecurityKeyMessageCallback& message_callback,
    base::OnceClosure error_callback) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  message_callback_ = message_callback;
  error_callback_ = std::move(error_callback);

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
    if (!ReadFromStream(std::data(message_data), message_data.size())) {
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
  base::span<uint8_t> buffer_span =
      base::as_writable_bytes(UNSAFE_TODO(base::span(buffer, bytes_to_read)));
  do {
    std::optional<size_t> read_result =
        read_stream_.ReadAtCurrentPosNoBestEffort(buffer_span);
    if (!read_result.has_value()) {
      LOG(ERROR) << "Failed to read from stream, ReadAtCurrentPos failed";
      return false;
    }
    if (*read_result == 0) {
      // 0 means EOF which is normal and should not be logged as an error.
      return false;
    }
    buffer_span = buffer_span.subspan(*read_result);
  } while (!buffer_span.empty());
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
  std::move(error_callback_).Run();
}

}  // namespace remoting
