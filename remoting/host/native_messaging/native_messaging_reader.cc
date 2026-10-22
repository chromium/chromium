// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/native_messaging/native_messaging_reader.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/numerics/byte_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/threading/platform_thread.h"
#include "base/win/scoped_handle.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

// uint32_t is specified in the protocol as the type for the message header.
typedef uint32_t MessageLengthType;

constexpr size_t kMessageHeaderSize = sizeof(MessageLengthType);

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

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core();

  // Reads a message from the Native Messaging client and passes it to
  // |message_callback_| on the originating thread. Called on the reader thread.
  void ReadMessage();

  void StopReading() { stop_reading_.store(true); }
  bool in_read() const { return in_read_.load(); }

 private:
  // Reads data from |read_stream_| into |buffer| while tracking |in_read_| and
  // checking |stop_reading_|.
  std::optional<size_t> ReadData(base::span<uint8_t> buffer);

  // Notify the reader's EOF callback when an error occurs or EOF is reached.
  void NotifyEof();

  base::File read_stream_;

  base::WeakPtr<NativeMessagingReader> reader_;

  // Used to post the caller-supplied reader callbacks on the caller thread.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Used to DCHECK that the reader code executes on the correct thread.
  scoped_refptr<base::SequencedTaskRunner> read_task_runner_;

  std::atomic<bool> stop_reading_{false};
  std::atomic<bool> in_read_{false};
};

NativeMessagingReader::Core::Core(
    base::File file,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SequencedTaskRunner> read_task_runner,
    base::WeakPtr<NativeMessagingReader> reader)
    : read_stream_(std::move(file)),
      reader_(reader),
      caller_task_runner_(caller_task_runner),
      read_task_runner_(read_task_runner) {}

NativeMessagingReader::Core::~Core() = default;

void NativeMessagingReader::Core::ReadMessage() {
  DCHECK(read_task_runner_->RunsTasksInCurrentSequence());

  while (!stop_reading_.load()) {
    std::array<uint8_t, kMessageHeaderSize> message_length_bytes;
    std::optional<size_t> read_result = ReadData(message_length_bytes);
    if (read_result != kMessageHeaderSize) {
      if (!stop_reading_.load()) {
        // 0 means EOF which is normal and should not be logged as an error.
        if (!read_result || *read_result != 0) {
          LOG(ERROR) << "Failed to read message header, read returned "
                     << read_result.value_or(-1);
        }
        NotifyEof();
      }
      return;
    }

    MessageLengthType message_length =
        base::U32FromNativeEndian(message_length_bytes);

    if (message_length > kMaximumMessageSize) {
      LOG(ERROR) << "Message size too large: " << message_length;
      NotifyEof();
      return;
    }

    std::string message_json(message_length, '\0');
    read_result = ReadData(base::as_writable_byte_span(message_json));
    if (read_result != message_length) {
      if (!stop_reading_.load()) {
        LOG(ERROR) << "Failed to read message body, read returned "
                   << read_result.value_or(-1);
        NotifyEof();
      }
      return;
    }

    std::optional<base::Value> message = base::JSONReader::Read(
        message_json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    if (!message) {
      LOG(ERROR) << "Failed to parse JSON message: " << message_json;
      NotifyEof();
      return;
    }

    // Notify callback of new message.
    caller_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&NativeMessagingReader::InvokeMessageCallback,
                                  reader_, std::move(*message)));
  }
}

std::optional<size_t> NativeMessagingReader::Core::ReadData(
    base::span<uint8_t> buffer) {
  DCHECK(read_task_runner_->RunsTasksInCurrentSequence());
  // Set |in_read_| before checking |stop_reading_| using default sequential
  // consistency. This guarantees that after ~NativeMessagingReader() calls
  // StopReading(), either this check sees |stop_reading_| == true and skips the
  // blocking read, or ~NativeMessagingReader() sees |in_read_| == true and
  // cancels the synchronous I/O until ReadAtCurrentPos() returns.
  in_read_.store(true);
  if (stop_reading_.load()) {
    in_read_.store(false);
    return std::nullopt;
  }
  std::optional<size_t> result = read_stream_.ReadAtCurrentPos(buffer);
  in_read_.store(false);
  return result;
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
  core_ = std::make_unique<Core>(
      std::move(file), base::SingleThreadTaskRunner::GetCurrentDefault(),
      read_task_runner_, weak_factory_.GetWeakPtr());
}

NativeMessagingReader::~NativeMessagingReader() {
  core_->StopReading();

#if BUILDFLAG(IS_WIN)
  // The ReadMessage() method uses a blocking read (on all platforms) which
  // causes a deadlock if the owning thread attempts to destroy this object
  // while there is a read operation pending.
  // On POSIX platforms, closing the write end of the pipe causes the Chrome
  // process to close the read end so that this class can be cleaned up.
  // On Windows, closing the write end of the pipe does nothing as the parent
  // process is cmd.exe which doesn't care. Thus, the read end of the pipe
  // remains open, the read operation is blocked, and we hang in the d'tor.
  //
  // CancelSynchronousIo() only cancels an I/O request that is already pending
  // in the kernel. If the reader thread passed the |stop_reading_| check in
  // ReadData() but has not yet entered the kernel ReadFile() call,
  // CancelSynchronousIo() returns ERROR_NOT_FOUND. We must keep retrying while
  // |in_read_| is true; breaking out early would leave the subsequent
  // ReadFile() uncanceled and cause reader_thread_'s destructor (which joins
  // the thread) to deadlock.
  base::PlatformThreadId thread_id = reader_thread_.GetThreadId();
  base::win::ScopedHandle thread_handle(
      OpenThread(THREAD_TERMINATE, /*bInheritHandle=*/false, thread_id.raw()));
  while (core_->in_read()) {
    if (!CancelSynchronousIo(thread_handle.Get()) &&
        GetLastError() != ERROR_NOT_FOUND) {
      PLOG(ERROR) << "CancelSynchronousIo() failed";
      break;
    }
    if (core_->in_read()) {
      base::PlatformThread::YieldCurrentThread();
    }
  }
#endif  // BUILDFLAG(IS_WIN)

  read_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void NativeMessagingReader::Start(const MessageCallback& message_callback,
                                  base::OnceClosure eof_callback) {
  message_callback_ = message_callback;
  eof_callback_ = std::move(eof_callback);

  // base::Unretained is safe since |core_| is only deleted via the
  // DeleteSoon task which is posted from this class's dtor.
  read_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NativeMessagingReader::Core::ReadMessage,
                                base::Unretained(core_.get())));
}

void NativeMessagingReader::InvokeMessageCallback(base::Value message) {
  message_callback_.Run(std::move(message));
}

void NativeMessagingReader::InvokeEofCallback() {
  std::move(eof_callback_).Run();
}

}  // namespace remoting
