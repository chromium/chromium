// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/io_completion_port_drainer.h"

#include <windows.h>

#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/unguessable_token.h"

namespace remoting {

// static
std::unique_ptr<IoCompletionPortDrainer> IoCompletionPortDrainer::Create() {
  const std::wstring pipe_name = base::ASCIIToWide(
      base::StrCat({"\\\\.\\pipe\\remoting.iocp_drainer.",
                    base::NumberToString(::GetCurrentProcessId()), ".",
                    base::UnguessableToken::Create().ToString()}));

  // Pass null `SECURITY_ATTRIBUTES` to both `CreateNamedPipeW()` and
  // `CreateFileW()` so neither handle is ever inheritable, avoiding any window
  // where a concurrent `CreateProcess(bInheritHandles=TRUE)` on another thread
  // could leak a handle into a child process. If another local process managed
  // to guess the random pipe name and open the single client instance before
  // `CreateFileW()` runs, `CreateFileW()` fails cleanly with `ERROR_PIPE_BUSY`.
  constexpr DWORD kOpenMode = PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED |
                              FILE_FLAG_FIRST_PIPE_INSTANCE;
  constexpr DWORD kPipeMode =
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS;
  base::win::ScopedHandle write_handle(::CreateNamedPipeW(
      pipe_name.c_str(), kOpenMode, kPipeMode, /*nMaxInstances=*/1,
      /*nOutBufferSize=*/64, /*nInBufferSize=*/64, /*nDefaultTimeOut=*/0,
      /*lpSecurityAttributes=*/nullptr));
  if (!write_handle.is_valid()) {
    PLOG(ERROR) << "Failed to create completion-port drain pipe server";
    return nullptr;
  }

  constexpr DWORD kClientFlags =
      SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS | FILE_FLAG_OVERLAPPED;
  base::win::ScopedHandle read_handle(::CreateFileW(
      pipe_name.c_str(), GENERIC_READ, /*dwShareMode=*/0,
      /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING, kClientFlags,
      /*hTemplateFile=*/nullptr));
  if (!read_handle.is_valid()) {
    PLOG(ERROR) << "Failed to connect completion-port drain pipe client";
    return nullptr;
  }

  // Since the client end is already connected, `ConnectNamedPipe()` completes
  // synchronously with `ERROR_PIPE_CONNECTED`.
  if (::ConnectNamedPipe(write_handle.Get(), nullptr) ||
      ::GetLastError() != ERROR_PIPE_CONNECTED) {
    PLOG(ERROR) << "Failed to confirm completion-port drain pipe connection";
    return nullptr;
  }

  return base::WrapUnique(new IoCompletionPortDrainer(std::move(write_handle),
                                                      std::move(read_handle)));
}

IoCompletionPortDrainer::IoCompletionPortDrainer()
    : base::MessagePumpForIO::IOHandler(FROM_HERE) {
  DETACH_FROM_THREAD(io_thread_checker_);
}

IoCompletionPortDrainer::IoCompletionPortDrainer(
    base::win::ScopedHandle write_handle,
    base::win::ScopedHandle read_handle)
    : base::MessagePumpForIO::IOHandler(FROM_HERE),
      write_handle_(std::move(write_handle)),
      read_handle_(std::move(read_handle)) {
  DETACH_FROM_THREAD(io_thread_checker_);
}

IoCompletionPortDrainer::~IoCompletionPortDrainer() {
  // Destroying the drainer while a sentinel packet is still in flight on the
  // completion port would leave a dangling `IOHandler*` completion key in the
  // kernel.
  CHECK(!on_drained_);
}

bool IoCompletionPortDrainer::RegisterOnIoThread() {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  CHECK(!registered_);
  CHECK(write_handle_.is_valid());

  if (!base::CurrentIOThread::Get()->RegisterIOHandler(write_handle_.Get(),
                                                       this)) {
    LOG(ERROR) << "Failed to associate the drain pipe with a completion port";
    return false;
  }

  registered_ = true;
  return true;
}

void IoCompletionPortDrainer::Drain(base::OnceClosure on_drained) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  CHECK(registered_);
  CHECK(!drain_started_);
  CHECK(on_drained);
  CHECK(write_handle_.is_valid());

  drain_started_ = true;
  on_drained_ = std::move(on_drained);
  const BOOL ok =
      ::WriteFile(write_handle_.Get(), &sentinel_byte_, sizeof(sentinel_byte_),
                  nullptr, io_context_.GetOverlapped());
  // Crash rather than returning a failure: if no completion packet is queued,
  // `on_drained_` will never run and the caller would remain pinned forever.
  PCHECK(ok || ::GetLastError() == ERROR_IO_PENDING);
}

void IoCompletionPortDrainer::OnIOCompleted(
    base::MessagePumpForIO::IOContext* context,
    DWORD bytes_transferred,
    DWORD error) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  CHECK_EQ(context, &io_context_);
  CHECK(on_drained_);

  // `bytes_transferred` and `error` are intentionally ignored: even a failed
  // sentinel write proves the completion port has reached this position in the
  // FIFO queue.
  write_handle_.Close();
  read_handle_.Close();

  // Move `on_drained_` onto the stack before running it and do not touch
  // `this` or any member afterward: `on_drained` may destroy `this`
  // synchronously or post a task that destroys `this` concurrently on another
  // thread.
  std::move(on_drained_).Run();
}

}  // namespace remoting
