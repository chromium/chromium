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
#include <string>
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "net/server/http_connection.h"

const size_t kReceiveBufferSizeForDevTools = 100 * 1024 * 1024;  // 100Mb
const size_t kWritePacketSize = 1 << 16;
const int kReadFD = 3;
const int kWriteFD = 4;

namespace content {

namespace {

const char kDevToolsPipeHandlerReadThreadName[] =
    "DevToolsPipeHandlerReadThread";
const char kDevToolsPipeHandlerWriteThreadName[] =
    "DevToolsPipeHandlerWriteThread";

void WriteIntoPipe(int write_fd, const std::string& message) {
#if defined(OS_WIN)
  HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(write_fd));
#endif

  size_t total_written = 0;
  while (total_written < message.length()) {
    size_t length = message.length() - total_written;
    if (length > kWritePacketSize)
      length = kWritePacketSize;
#if defined(OS_WIN)
    DWORD result = 0;
    WriteFile(handle, message.data() + total_written,
              static_cast<DWORD>(length), &result, nullptr);
#else
    int result = write(write_fd, message.data() + total_written, length);
#endif
    if (!result) {
      LOG(ERROR) << "Could not write into pipe";
      return;
    }
    total_written += result;
  }
#if defined(OS_WIN)
  DWORD result = 0;
  WriteFile(handle, "\0", 1, &result, nullptr);
#else
  int result = write(write_fd, "\0", 1);
#endif
  if (!result) {
    LOG(ERROR) << "Could not write into pipe";
    return;
  }
}

}  // namespace

// PipeReader ------------------------------------------------------------------

class PipeReader {
 public:
  PipeReader(base::WeakPtr<DevToolsPipeHandler> devtools_handler, int read_fd);
  ~PipeReader() = default;
  void ReadLoop();

 private:
  bool HandleReadResult(int result);

  void ConnectionClosed();

  scoped_refptr<net::HttpConnection::ReadIOBuffer> read_buffer_;
  base::WeakPtr<DevToolsPipeHandler> devtools_handler_;
#if defined(OS_WIN)
  HANDLE read_handle_;
#else
  int read_fd_;
#endif
};

PipeReader::PipeReader(base::WeakPtr<DevToolsPipeHandler> devtools_handler,
                       int read_fd)
    : devtools_handler_(devtools_handler) {
#if defined(OS_WIN)
  read_handle_ = reinterpret_cast<HANDLE>(_get_osfhandle(read_fd));
#else
  read_fd_ = read_fd;
#endif

  read_buffer_ = new net::HttpConnection::ReadIOBuffer();
  read_buffer_->set_max_buffer_size(kReceiveBufferSizeForDevTools);
}

void PipeReader::ReadLoop() {
  while (true) {
    if (read_buffer_->RemainingCapacity() == 0 &&
        !read_buffer_->IncreaseCapacity()) {
      LOG(ERROR) << "Connection closed, not enough capacity";
      break;
    }

#if defined(OS_WIN)
    DWORD result = 0;
    ReadFile(read_handle_, read_buffer_->data(),
             read_buffer_->RemainingCapacity(), &result, nullptr);
#else
    int result =
        read(read_fd_, read_buffer_->data(), read_buffer_->RemainingCapacity());
#endif

    if (!HandleReadResult(result))
      break;
  }

  ConnectionClosed();
}

bool PipeReader::HandleReadResult(int result) {
  if (result == 0) {
    LOG(ERROR) << "Connection terminated while reading from pipe";
    return false;
  }

  read_buffer_->DidRead(result);

  // Go over the last read chunk, look for \0, extract messages.
  int offset = 0;
  for (int i = read_buffer_->GetSize() - result; i < read_buffer_->GetSize();
       ++i) {
    if (read_buffer_->StartOfBuffer()[i] == '\0') {
      std::string str(read_buffer_->StartOfBuffer() + offset, i - offset);

      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&DevToolsPipeHandler::HandleMessage, devtools_handler_,
                         std::move(str)));
      offset = i + 1;
    }
  }
  if (offset)
    read_buffer_->DidConsume(offset);
  return true;
}

void PipeReader::ConnectionClosed() {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&DevToolsPipeHandler::Shutdown, devtools_handler_));
}

// DevToolsPipeHandler ---------------------------------------------------

DevToolsPipeHandler::DevToolsPipeHandler()
    : read_fd_(kReadFD), write_fd_(kWriteFD), weak_factory_(this) {
  read_thread_.reset(new base::Thread(kDevToolsPipeHandlerReadThreadName));
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
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

  pipe_reader_.reset(new PipeReader(weak_factory_.GetWeakPtr(), read_fd_));
  base::TaskRunner* task_runner = read_thread_->task_runner().get();
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&PipeReader::ReadLoop,
                                       base::Unretained(pipe_reader_.get())));
}

void DevToolsPipeHandler::Shutdown() {
  // Is there is no read thread, there is nothing, it is safe to proceed.
  if (!read_thread_)
    return;

  // If there is no write thread, only take care of the read thread.
  if (!write_thread_) {
    base::PostTaskWithTraits(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
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
  CloseHandle(reinterpret_cast<HANDLE>(_get_osfhandle(read_fd_)));
  CloseHandle(reinterpret_cast<HANDLE>(_get_osfhandle(write_fd_)));
#else
  shutdown(read_fd_, SHUT_RDWR);
  shutdown(write_fd_, SHUT_RDWR);
#endif

  // Post PipeReader and WeakPtr factory destruction on the reader thread.
  read_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce([](PipeReader* reader) { delete reader; },
                                pipe_reader_.release()));

  // Post background task that would join and destroy the threads.
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
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
      FROM_HERE, base::BindOnce(&WriteIntoPipe, write_fd_, std::move(message)));
}

void DevToolsPipeHandler::AgentHostClosed(DevToolsAgentHost* agent_host) {}

}  // namespace content
