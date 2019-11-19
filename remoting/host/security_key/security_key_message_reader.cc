// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_message_reader.h"

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

SecurityKeyMessageReader::SecurityKeyMessageReader(base::File input_file)
    : read_stream_(std::move(input_file)),
      reader_thread_("SecurityKeyMessageReader"),
      weak_factory_(this) {
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  reader_thread_.StartWithOptions(options);

  read_task_runner_ = reader_thread_.task_runner();
  main_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

SecurityKeyMessageReader::~SecurityKeyMessageReader() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  // In order to ensure the reader thread is stopped cleanly, we close the
  // stream it is blocking on and then wait for the thread to exit.
  read_stream_.Close();
  reader_thread_.Stop();
}

void SecurityKeyMessageReader::Start(
    SecurityKeyMessageCallback message_callback,
    base::Closure error_callback) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  message_callback_ = message_callback;
  error_callback_ = error_callback;

  // base::Unretained is safe since this class owns the thread running this task
  // which will be destroyed before this instance is.
  read_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SecurityKeyMessageReader::ReadMessage,
                                base::Unretained(this)));
}

void SecurityKeyMessageReader::ReadMessage() {
  DCHECK(read_task_runner_->RunsTasksInCurrentSequence());

  while (true) {
    if (!read_stream_.IsValid()) {
      LOG(ERROR) << "Cannot read from invalid stream.";
      NotifyError();
      return;
    }

    // Read the message header to retrieve the remaining message length.
    uint32_t total_message_size_bytes;
    int read_result = read_stream_.ReadAtCurrentPos(
        reinterpret_cast<char*>(&total_message_size_bytes),
        SecurityKeyMessage::kHeaderSizeBytes);
    if (read_result != SecurityKeyMessage::kHeaderSizeBytes) {
      // 0 means EOF which is normal and should not be logged as an error.
      if (read_result != 0) {
        LOG(ERROR) << "Failed to read message header, read returned "
                   << read_result;
      }
      NotifyError();
      return;
    }

    if (!SecurityKeyMessage::IsValidMessageSize(total_message_size_bytes)) {
      LOG(ERROR) << "Message size too large: " << total_message_size_bytes;
      NotifyError();
      return;
    }

    std::string message_data(total_message_size_bytes, '\0');
    read_result = read_stream_.ReadAtCurrentPos(base::data(message_data),
                                                total_message_size_bytes);
    // The static cast is safe as we know the value is smaller than max int.
    if (read_result != static_cast<int>(total_message_size_bytes)) {
      LOG(ERROR) << "Failed to read message: " << read_result;
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
        base::BindOnce(&SecurityKeyMessageReader::InvokeMessageCallback,
                       weak_factory_.GetWeakPtr(), std::move(message)));
  }
}

void SecurityKeyMessageReader::NotifyError() {
  DCHECK(read_task_runner_->RunsTasksInCurrentSequence());

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SecurityKeyMessageReader::InvokeErrorCallback,
                                weak_factory_.GetWeakPtr()));
}

void SecurityKeyMessageReader::InvokeMessageCallback(
    std::unique_ptr<SecurityKeyMessage> message) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  message_callback_.Run(std::move(message));
}

void SecurityKeyMessageReader::InvokeErrorCallback() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  error_callback_.Run();
}

}  // namespace remoting
