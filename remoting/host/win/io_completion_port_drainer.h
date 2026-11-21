// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_IO_COMPLETION_PORT_DRAINER_H_
#define REMOTING_HOST_WIN_IO_COMPLETION_PORT_DRAINER_H_

#include <stdint.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/threading/thread_checker.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"

namespace remoting {

// Enqueues a sentinel I/O completion packet onto a completion-port-backed
// `MessagePumpForIO` thread's Windows completion port and invokes a callback
// once all completion packets already queued in the kernel when `Drain()` runs
// have been dispatched.
//
// Requires a completion-port `MessagePumpForIO`; cannot be used on
// `MessagePumpForIOEventImpl`, where `RegisterIOHandler()` is `NOTREACHED()`.
//
// Scope of the ordering guarantee:
// - Only packets already in the kernel completion port queue when `Drain()`
//   issues its sentinel `WriteFile()` are ordered ahead of `on_drained`. I/O
//   that is still pending, or notifications the kernel posts later, will land
//   behind the sentinel.
// - "Dispatched" means each prior packet's `IOHandler::OnIOCompleted()` has
//   returned, not that tasks posted by those handlers to other threads have
//   run.
//
// Posting a regular task to an I/O `SingleThreadTaskRunner` does not provide
// this ordering because `MessagePumpForIO` interleaves application tasks with
// individual completion packets and can run posted tasks ahead of packets
// still waiting in the kernel completion port queue.
class IoCompletionPortDrainer : public base::MessagePumpForIO::IOHandler {
 public:
  // Creates a drainer with pre-allocated, non-inheritable overlapped pipe
  // handles. May be called on any thread. Returns nullptr on handle creation
  // failure.
  static std::unique_ptr<IoCompletionPortDrainer> Create();

  IoCompletionPortDrainer(const IoCompletionPortDrainer&) = delete;
  IoCompletionPortDrainer& operator=(const IoCompletionPortDrainer&) = delete;

  // May be destroyed on any thread before `Drain()` is called, synchronously
  // inside `on_drained`, or on any thread after `on_drained` has been invoked
  // (e.g., via a task posted by `on_drained`).
  //
  // There is no way to cancel or abandon an in-flight drain. If the I/O pump
  // stops before the sentinel packet is dispatched, `on_drained` will never
  // run and the drainer must be leaked; destroying it while a drain is in
  // flight hits a `CHECK` to prevent a dangling `IOHandler*` completion key in
  // the kernel.
  ~IoCompletionPortDrainer() override;

  // Associates the drainer with the current completion-port-backed
  // `MessagePumpForIO` thread's completion port. Must be called on that I/O
  // thread before `Drain()`.
  virtual bool RegisterOnIoThread();

  // Enqueues a sentinel completion packet onto the completion port and runs
  // `on_drained` on the I/O thread once that packet is dequeued (after every
  // completion packet already queued when `WriteFile()` runs has returned from
  // `OnIOCompleted()`). Must be called at most once, on the same I/O thread
  // where `RegisterOnIoThread()` succeeded. Cannot be cancelled once started;
  // `this` must outlive the invocation of `on_drained` (`on_drained` itself
  // may destroy `this` or trigger its destruction on another thread).
  virtual void Drain(base::OnceClosure on_drained);

 protected:
  // Protected default constructor for test doubles that override
  // `RegisterOnIoThread()` and `Drain()` without allocating OS pipe handles.
  IoCompletionPortDrainer();

 private:
  IoCompletionPortDrainer(base::win::ScopedHandle write_handle,
                          base::win::ScopedHandle read_handle);

  // `base::MessagePumpForIO::IOHandler` implementation.
  void OnIOCompleted(base::MessagePumpForIO::IOContext* context,
                     DWORD bytes_transferred,
                     DWORD error) override;

  base::win::ScopedHandle write_handle_;
  base::win::ScopedHandle read_handle_;
  base::MessagePumpForIO::IOContext io_context_;
  uint8_t sentinel_byte_ = 1;
  bool registered_ = false;
  bool drain_started_ = false;
  base::OnceClosure on_drained_;

  THREAD_CHECKER(io_thread_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_IO_COMPLETION_PORT_DRAINER_H_
