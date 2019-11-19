// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_pipe_handler.h"

#if defined(OS_WIN)
#include <io.h>
#include <windows.h>
#else
#include <sys/socket.h>
#endif

#include <stdio.h>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_pump_type.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/common/content_switches.h"
#include "net/server/http_connection.h"
#include "third_party/inspector_protocol/crdtp/cbor.h"

const size_t kReceiveBufferSizeForDevTools = 100 * 1024 * 1024;  // 100Mb
const size_t kWritePacketSize = 1 << 16;
const int kReadFD = 3;
const int kWriteFD = 4;

// Our CBOR (RFC 7049) based format starts with a tag 24 indicating
// an envelope, that is, a byte string which as payload carries the
// entire remaining message. Thereby, the length of the byte string
// also tells us the message size on the wire.
// The details of the encoding are implemented in
// third_party/inspector_protocol/crdtp/encoding.h.
namespace content {

class PipeReaderBase {
 public:
  explicit PipeReaderBase(base::WeakPtr<DevToolsPipeHandler> devtools_handler,
                          int read_fd)
      : devtools_handler_(std::move(devtools_handler)) {
#if defined(OS_WIN)
    read_handle_ = reinterpret_cast<HANDLE>(_get_osfhandle(read_fd));
#else
    read_fd_ = read_fd;
#endif
  }

  virtual ~PipeReaderBase() = default;

  void ReadLoop() {
    ReadLoopInternal();
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&DevToolsPipeHandler::Shutdown, devtools_handler_));
  }

 protected:
  virtual void ReadLoopInternal() = 0;

  size_t ReadBytes(void* buffer, size_t size, bool exact_size) {
    size_t bytes_read = 0;
    while (bytes_read < size) {
#if defined(OS_WIN)
      DWORD size_read = 0;
      bool had_error =
          !ReadFile(read_handle_, static_cast<char*>(buffer) + bytes_read,
                    size - bytes_read, &size_read, nullptr);
#else
      int size_read = read(read_fd_, static_cast<char*>(buffer) + bytes_read,
                           size - bytes_read);
      if (size_read < 0 && errno == EINTR)
        continue;
      bool had_error = size_read <= 0;
#endif
      if (had_error) {
        LOG(ERROR) << "Connection terminated while reading from pipe";
        return 0;
      }
      bytes_read += size_read;
      if (!exact_size)
        break;
    }
    return bytes_read;
  }

  void HandleMessage(std::string buffer) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&DevToolsPipeHandler::HandleMessage,
                                  devtools_handler_, std::move(buffer)));
  }

 protected:
  base::WeakPtr<DevToolsPipeHandler> devtools_handler_;
#if defined(OS_WIN)
  HANDLE read_handle_;
#else
  int read_fd_;
#endif
};

namespace {

const char kDevToolsPipeHandlerReadThreadName[] =
    "DevToolsPipeHandlerReadThread";
const char kDevToolsPipeHandlerWriteThreadName[] =
    "DevToolsPipeHandlerWriteThread";

void WriteBytes(int write_fd, const char* bytes, size_t size) {
#if defined(OS_WIN)
  HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(write_fd));
#endif

  size_t total_written = 0;
  while (total_written < size) {
    size_t length = size - total_written;
    if (length > kWritePacketSize)
      length = kWritePacketSize;
#if defined(OS_WIN)
    DWORD bytes_written = 0;
    bool had_error =
        !WriteFile(handle, bytes + total_written, static_cast<DWORD>(length),
                   &bytes_written, nullptr);
#else
    int bytes_written = write(write_fd, bytes + total_written, length);
    if (bytes_written < 0 && errno == EINTR)
      continue;
    bool had_error = bytes_written <= 0;
#endif
    if (had_error) {
      LOG(ERROR) << "Could not write into pipe";
      return;
    }
    total_written += bytes_written;
  }
}

void WriteIntoPipeASCIIZ(int write_fd, const std::string& message) {
  WriteBytes(write_fd, message.data(), message.size());
  WriteBytes(write_fd, "\0", 1);
}

void WriteIntoPipeCBOR(int write_fd, const std::string& message) {
  DCHECK(crdtp::cbor::IsCBORMessage(crdtp::SpanFrom(message)));

  WriteBytes(write_fd, message.data(), message.size());
}

class PipeReaderASCIIZ : public PipeReaderBase {
 public:
  PipeReaderASCIIZ(base::WeakPtr<DevToolsPipeHandler> devtools_handler,
                   int read_fd)
      : PipeReaderBase(std::move(devtools_handler), read_fd) {
    read_buffer_ = new net::HttpConnection::ReadIOBuffer();
    read_buffer_->set_max_buffer_size(kReceiveBufferSizeForDevTools);
  }

 private:
  void ReadLoopInternal() override {
    while (true) {
      if (read_buffer_->RemainingCapacity() == 0 &&
          !read_buffer_->IncreaseCapacity()) {
        LOG(ERROR) << "Connection closed, not enough capacity";
        break;
      }

      size_t bytes_read = ReadBytes(read_buffer_->data(),
                                    read_buffer_->RemainingCapacity(), false);
      if (!bytes_read)
        break;
      read_buffer_->DidRead(bytes_read);

      // Go over the last read chunk, look for \0, extract messages.
      int offset = 0;
      for (int i = read_buffer_->GetSize() - bytes_read;
           i < read_buffer_->GetSize(); ++i) {
        if (read_buffer_->StartOfBuffer()[i] == '\0') {
          std::string str(read_buffer_->StartOfBuffer() + offset, i - offset);
          HandleMessage(std::move(str));
          offset = i + 1;
        }
      }
      if (offset)
        read_buffer_->DidConsume(offset);
    }
  }

  scoped_refptr<net::HttpConnection::ReadIOBuffer> read_buffer_;
};

class PipeReaderCBOR : public PipeReaderBase {
 public:
  PipeReaderCBOR(base::WeakPtr<DevToolsPipeHandler> devtools_handler,
                 int read_fd)
      : PipeReaderBase(std::move(devtools_handler), read_fd) {}

 private:
  static uint32_t UInt32FromCBOR(const uint8_t* buf) {
    return (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
  }

  void ReadLoopInternal() override {
    while (true) {
      const size_t kHeaderSize = 6;  // tag? type length*4
      std::string buffer(kHeaderSize, '\0');
      if (!ReadBytes(&buffer.front(), kHeaderSize, true))
        break;
      const uint8_t* prefix = reinterpret_cast<const uint8_t*>(buffer.data());
      if (prefix[0] != crdtp::cbor::InitialByteForEnvelope() ||
          prefix[1] != crdtp::cbor::InitialByteFor32BitLengthByteString()) {
        LOG(ERROR) << "Unexpected start of CBOR envelope " << prefix[0] << ","
                   << prefix[1];
        return;
      }
      uint32_t msg_size = UInt32FromCBOR(prefix + 2);
      buffer.resize(kHeaderSize + msg_size, '\0');
      if (!ReadBytes(&buffer.front() + kHeaderSize, msg_size, true))
        return;
      HandleMessage(std::move(buffer));
    }
  }
};

}  // namespace

// DevToolsPipeHandler ---------------------------------------------------

DevToolsPipeHandler::DevToolsPipeHandler()
    : read_fd_(kReadFD), write_fd_(kWriteFD) {
  read_thread_.reset(new base::Thread(kDevToolsPipeHandlerReadThreadName));
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  if (!read_thread_->StartWithOptions(options)) {
    read_thread_.reset();
    Shutdown();
    return;
  }

  write_thread_.reset(new base::Thread(kDevToolsPipeHandlerWriteThreadName));
  if (!write_thread_->StartWithOptions(options)) {
    write_thread_.reset();
    Shutdown();
    return;
  }

  browser_target_ = DevToolsAgentHost::CreateForBrowser(
      nullptr, DevToolsAgentHost::CreateServerSocketCallback());
  browser_target_->AttachClient(this);

  std::string str_mode = base::ToLowerASCII(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kRemoteDebuggingPipe));
  mode_ = str_mode == "cbor" ? DevToolsPipeHandler::ProtocolMode::kCBOR
                             : DevToolsPipeHandler::ProtocolMode::kASCIIZ;

  switch (mode_) {
    case ProtocolMode::kASCIIZ:
      pipe_reader_ = std::make_unique<PipeReaderASCIIZ>(
          weak_factory_.GetWeakPtr(), read_fd_);
      break;

    case ProtocolMode::kCBOR:
      pipe_reader_ = std::make_unique<PipeReaderCBOR>(
          weak_factory_.GetWeakPtr(), read_fd_);
      break;
  }
  read_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&PipeReaderBase::ReadLoop,
                                base::Unretained(pipe_reader_.get())));
}

void DevToolsPipeHandler::Shutdown() {
  if (shutting_down_)
    return;
  shutting_down_ = true;
  // Is there is no read thread, there is nothing, it is safe to proceed.
  if (!read_thread_)
    return;

  // If there is no write thread, only take care of the read thread.
  if (!write_thread_) {
    base::PostTask(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce([](base::Thread* rthread) { delete rthread; },
                       read_thread_.release()));
    return;
  }

  // There were threads, disconnect from the target.
  DCHECK(browser_target_);
  browser_target_->DetachClient(this);
  browser_target_ = nullptr;

// Concurrently discard the pipe handles to successfully join threads.
#if defined(OS_WIN)
  HANDLE read_handle = reinterpret_cast<HANDLE>(_get_osfhandle(read_fd_));
  // Cancel pending synchronous read.
  CancelIoEx(read_handle, NULL);
  CloseHandle(read_handle);
  CloseHandle(reinterpret_cast<HANDLE>(_get_osfhandle(write_fd_)));
#else
  shutdown(read_fd_, SHUT_RDWR);
  shutdown(write_fd_, SHUT_RDWR);
#endif

  // Post PipeReader and WeakPtr factory destruction on the reader thread.
  read_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce([](PipeReaderBase* reader) { delete reader; },
                                pipe_reader_.release()));

  // Post background task that would join and destroy the threads.
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](base::Thread* rthread, base::Thread* wthread) {
            delete rthread;
            delete wthread;
          },
          read_thread_.release(), write_thread_.release()));
}

DevToolsPipeHandler::~DevToolsPipeHandler() {
  Shutdown();
}

void DevToolsPipeHandler::HandleMessage(const std::string& message) {
  if (browser_target_)
    browser_target_->DispatchProtocolMessage(this, message);
}

void DevToolsPipeHandler::DetachFromTarget() {}

void DevToolsPipeHandler::DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                                                  const std::string& message) {
  if (!write_thread_)
    return;
  base::TaskRunner* task_runner = write_thread_->task_runner().get();
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(mode_ == ProtocolMode::kASCIIZ ? WriteIntoPipeASCIIZ
                                                    : WriteIntoPipeCBOR,
                     write_fd_, std::move(message)));
}

void DevToolsPipeHandler::AgentHostClosed(DevToolsAgentHost* agent_host) {}

bool DevToolsPipeHandler::UsesBinaryProtocol() {
  return mode_ == ProtocolMode::kCBOR;
}

}  // namespace content
