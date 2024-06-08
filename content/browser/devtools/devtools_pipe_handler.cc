// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/devtools/devtools_pipe_handler.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <io.h>
#include <stdlib.h>
#else
#include <sys/socket.h>
#endif

#include <stdio.h>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/string_util.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/common/content_switches.h"
#include "net/server/http_connection.h"
#include "third_party/inspector_protocol/crdtp/cbor.h"

const size_t kReceiveBufferSizeForDevTools = 100 * 1024 * 1024;  // 100Mb
const size_t kWritePacketSize = 1 << 16;

// Our CBOR (RFC 7049) based format starts with a tag 24 indicating
// an envelope, that is, a byte string which as payload carries the
// entire remaining message. Thereby, the length of the byte string
// also tells us the message size on the wire.
// The details of the encoding are implemented in
// third_party/inspector_protocol/crdtp/cbor.h.
namespace content {

namespace {
class PipeIOBase {
 public:
  explicit PipeIOBase(const char* thread_name)
      : thread_(new base::Thread(thread_name)) {}

  virtual ~PipeIOBase() = default;

  bool Start() {
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    if (!thread_->StartWithOptions(std::move(options)))
      return false;
    StartMainLoop();
    return true;
  }

  static void Shutdown(std::unique_ptr<PipeIOBase> pipe_io) {
    if (!pipe_io)
      return;
    auto thread = std::move(pipe_io->thread_);
    pipe_io->shutting_down_.Set();
    pipe_io->ClosePipe();
    // Post self destruction on the custom thread if it's running.
    if (thread->task_runner()) {
      thread->task_runner()->DeleteSoon(FROM_HERE, std::move(pipe_io));
    } else {
      pipe_io.reset();
    }
    // Post background task that would join and destroy the thread.
    base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
         base::WithBaseSyncPrimitives(), base::TaskPriority::BEST_EFFORT})
        ->DeleteSoon(FROM_HERE, std::move(thread));
  }

 protected:
  virtual void StartMainLoop() {}
  virtual void ClosePipe() = 0;

  std::unique_ptr<base::Thread> thread_;
  base::AtomicFlag shutting_down_;
};

#if BUILDFLAG(IS_WIN)
// Temporary CRT parameter validation error handler override that allows
//  _get_osfhandle() to return INVALID_HANDLE_VALUE instead of crashing.
class ScopedInvalidParameterHandlerOverride {
 public:
  ScopedInvalidParameterHandlerOverride()
      : prev_invalid_parameter_handler_(
            _set_thread_local_invalid_parameter_handler(
                InvalidParameterHandler)) {}

  ScopedInvalidParameterHandlerOverride(
      const ScopedInvalidParameterHandlerOverride&) = delete;
  ScopedInvalidParameterHandlerOverride& operator=(
      const ScopedInvalidParameterHandlerOverride&) = delete;

  ~ScopedInvalidParameterHandlerOverride() {
    _set_thread_local_invalid_parameter_handler(
        prev_invalid_parameter_handler_);
  }

 private:
  // A do nothing invalid parameter handler that causes CRT routine to return
  // error to the caller.
  static void InvalidParameterHandler(const wchar_t* expression,
                                      const wchar_t* function,
                                      const wchar_t* file,
                                      unsigned int line,
                                      uintptr_t reserved) {}

  const _invalid_parameter_handler prev_invalid_parameter_handler_;
};

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

class PipeReaderBase : public PipeIOBase {
 public:
  PipeReaderBase(base::WeakPtr<DevToolsPipeHandler> devtools_handler,
                 int read_fd)
      : PipeIOBase("DevToolsPipeHandlerReadThread"),
        devtools_handler_(std::move(devtools_handler)),
        read_fd_(read_fd) {
#if BUILDFLAG(IS_WIN)
    ScopedInvalidParameterHandlerOverride invalid_parameter_handler_override;
    read_handle_ = reinterpret_cast<HANDLE>(_get_osfhandle(read_fd));
#endif
  }

 protected:
  void StartMainLoop() override {
    thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&PipeReaderBase::ReadLoop, base::Unretained(this)));
  }

  void ClosePipe() override {
// Concurrently discard the pipe handles to successfully join threads.
#if BUILDFLAG(IS_WIN)
    // Cancel pending synchronous read.
    CancelIoEx(read_handle_, nullptr);
    ScopedInvalidParameterHandlerOverride invalid_parameter_handler_override;
    _close(read_fd_);
    read_handle_ = INVALID_HANDLE_VALUE;
#else
    shutdown(read_fd_, SHUT_RDWR);
#endif
  }

  virtual void ReadLoopInternal() = 0;

  size_t ReadBytes(void* buffer, size_t size, bool exact_size) {
    size_t bytes_read = 0;
    while (bytes_read < size) {
#if BUILDFLAG(IS_WIN)
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
        if (!shutting_down_.IsSet()) {
          LOG(ERROR) << "Connection terminated while reading from pipe";
          GetUIThreadTaskRunner({})->PostTask(
              FROM_HERE, base::BindOnce(&DevToolsPipeHandler::OnDisconnect,
                                        devtools_handler_));
        }
        return 0;
      }
      bytes_read += size_read;
      if (!exact_size)
        break;
    }
    return bytes_read;
  }

  void HandleMessage(std::vector<uint8_t> message) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&DevToolsPipeHandler::HandleMessage,
                                  devtools_handler_, std::move(message)));
  }

 private:
  void ReadLoop() {
    ReadLoopInternal();
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&DevToolsPipeHandler::Shutdown, devtools_handler_));
  }

  base::WeakPtr<DevToolsPipeHandler> devtools_handler_;
  int read_fd_;
#if BUILDFLAG(IS_WIN)
  HANDLE read_handle_;
#endif
};

class PipeWriterBase : public PipeIOBase {
 public:
  explicit PipeWriterBase(int write_fd)
      : PipeIOBase("DevToolsPipeHandlerWriteThread"), write_fd_(write_fd) {
#if BUILDFLAG(IS_WIN)
    ScopedInvalidParameterHandlerOverride invalid_parameter_handler_override;
    write_handle_ = reinterpret_cast<HANDLE>(_get_osfhandle(write_fd));
#endif
  }

  void Write(base::span<const uint8_t> message) {
    base::TaskRunner* task_runner = thread_->task_runner().get();
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&PipeWriterBase::WriteIntoPipe, base::Unretained(this),
                       std::string(message.begin(), message.end())));
  }

 protected:
  void ClosePipe() override {
#if BUILDFLAG(IS_WIN)
    ScopedInvalidParameterHandlerOverride invalid_parameter_handler_override;
    _close(write_fd_);
    write_handle_ = INVALID_HANDLE_VALUE;
#else
    shutdown(write_fd_, SHUT_RDWR);
#endif
  }

  virtual void WriteIntoPipe(std::string message) = 0;

  void WriteBytes(const char* bytes, size_t size) {
    size_t total_written = 0;
    while (total_written < size) {
      size_t length = size - total_written;
      if (length > kWritePacketSize)
        length = kWritePacketSize;
#if BUILDFLAG(IS_WIN)
      DWORD bytes_written = 0;
      bool had_error =
          !WriteFile(write_handle_, bytes + total_written,
                     static_cast<DWORD>(length), &bytes_written, nullptr);
#else
      int bytes_written = write(write_fd_, bytes + total_written, length);
      if (bytes_written < 0 && errno == EINTR)
        continue;
      bool had_error = bytes_written <= 0;
#endif
      if (had_error) {
        if (!shutting_down_.IsSet())
          LOG(ERROR) << "Could not write into pipe";
        return;
      }
      total_written += bytes_written;
    }
  }

 private:
  int write_fd_;
#if BUILDFLAG(IS_WIN)
  HANDLE write_handle_;
#endif
};

namespace {

class PipeWriterASCIIZ : public PipeWriterBase {
 public:
  explicit PipeWriterASCIIZ(int write_fd) : PipeWriterBase(write_fd) {}

  void WriteIntoPipe(std::string message) override {
    WriteBytes(message.data(), message.size());
    WriteBytes("\0", 1);
  }
};

class PipeWriterCBOR : public PipeWriterBase {
 public:
  explicit PipeWriterCBOR(int write_fd) : PipeWriterBase(write_fd) {}

  void WriteIntoPipe(std::string message) override {
    DCHECK(crdtp::cbor::IsCBORMessage(crdtp::SpanFrom(message)));
    WriteBytes(message.data(), message.size());
  }
};

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
          HandleMessage(
              std::vector<uint8_t>(read_buffer_->StartOfBuffer() + offset,
                                   read_buffer_->StartOfBuffer() + i));
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
      const size_t kPeekSize =
          8;  // tag tag_type? byte_string length*4 map_start
      std::vector<uint8_t> buffer(kPeekSize);
      if (!ReadBytes(&buffer.front(), kPeekSize, true))
        break;
      auto status_or_header = crdtp::cbor::EnvelopeHeader::ParseFromFragment(
          crdtp::SpanFrom(buffer));
      if (!status_or_header.ok()) {
        LOG(ERROR) << "Error parsing CBOR envelope: "
                   << status_or_header.status().ToASCIIString();
        return;
      }
      const size_t msg_size = (*status_or_header).outer_size();
      CHECK_GT(msg_size, kPeekSize);
      buffer.resize(msg_size);
      if (!ReadBytes(&buffer.front() + kPeekSize, msg_size - kPeekSize, true))
        return;
      HandleMessage(std::move(buffer));
    }
  }
};

}  // namespace

// DevToolsPipeHandler ---------------------------------------------------

DevToolsPipeHandler::DevToolsPipeHandler(int read_fd,
                                         int write_fd,
                                         base::OnceClosure on_disconnect)
    : on_disconnect_(std::move(on_disconnect)),
      read_fd_(read_fd),
      write_fd_(write_fd) {
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
      pipe_writer_ = std::make_unique<PipeWriterASCIIZ>(write_fd_);
      break;

    case ProtocolMode::kCBOR:
      pipe_reader_ = std::make_unique<PipeReaderCBOR>(
          weak_factory_.GetWeakPtr(), read_fd_);
      pipe_writer_ = std::make_unique<PipeWriterCBOR>(write_fd_);
      break;
  }
  if (!pipe_reader_->Start() || !pipe_writer_->Start())
    Shutdown();
}

void DevToolsPipeHandler::Shutdown() {
  if (shutting_down_)
    return;
  shutting_down_ = true;

  // Disconnect from the target.
  DCHECK(browser_target_);
  browser_target_->DetachClient(this);
  browser_target_ = nullptr;

  PipeIOBase::Shutdown(std::move(pipe_reader_));
  PipeIOBase::Shutdown(std::move(pipe_writer_));
}

DevToolsPipeHandler::~DevToolsPipeHandler() {
  Shutdown();
}

void DevToolsPipeHandler::HandleMessage(std::vector<uint8_t> message) {
  if (browser_target_)
    browser_target_->DispatchProtocolMessage(this, message);
}

void DevToolsPipeHandler::OnDisconnect() {
  if (on_disconnect_)
    std::move(on_disconnect_).Run();
}

void DevToolsPipeHandler::DispatchProtocolMessage(
    DevToolsAgentHost* agent_host,
    base::span<const uint8_t> message) {
  if (pipe_writer_)
    pipe_writer_->Write(message);
}

void DevToolsPipeHandler::AgentHostClosed(DevToolsAgentHost* agent_host) {}

bool DevToolsPipeHandler::UsesBinaryProtocol() {
  return mode_ == ProtocolMode::kCBOR;
}

bool DevToolsPipeHandler::AllowUnsafeOperations() {
  return true;
}

std::string DevToolsPipeHandler::GetTypeForMetrics() {
  return "RemoteDebugger";
}

}  // namespace content
