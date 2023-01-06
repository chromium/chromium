// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_EMBEDDER_SCOPED_IPC_SUPPORT_H_
#define MOJO_CORE_EMBEDDER_SCOPED_IPC_SUPPORT_H_

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace mojo {
namespace core {

// A simple class that initialized Mojo IPC support on construction and shuts
// down IPC support on destruction, optionally blocking the destructor on clean
// IPC shutdown completion.
class COMPONENT_EXPORT(MOJO_CORE_EMBEDDER) ScopedIPCSupport {
 public:
  // ShutdownPolicy is a type for specifying the desired Mojo IPC support
  // shutdown behavior used during ScopedIPCSupport destruction.
  //
  // What follows is a quick overview of why shutdown behavior is interesting
  // and how you might decide which behavior is right for your use case.
  //
  // BACKGROUND
  // ==========
  //
  // In order to facilitate efficient and reliable transfer of Mojo message pipe
  // endpoints across process boundaries, the underlying model for a message
  // pipe is actually a self-collapsing cycle of "ports." See
  // //mojo/core/ports for gritty implementation details.
  //
  // Ports are essentially globally unique identifiers used for system-wide
  // message routing. Every message pipe consists of at least two such ports:
  // the pipe's two concrete endpoints.
  //
  // When a message pipe endpoint is transferred over another message pipe, that
  // endpoint's port (which subsequently exists only internally with no
  // publicly-reachable handle) enters a transient proxying state for the
  // remainder of its lifetime. Once sufficient information has been
  // proagated throughout the system and this proxying port can be safely
  // bypassed, it is garbage-collected.
  //
  // If a process is terminated while hosting any active proxy ports, this
  // will necessarily break the message pipe(s) to which those ports belong.
  //
  // WHEN TO USE CLEAN SHUTDOWN
  // ==========================
  //
  // Consider three processes, A, B, and C. Suppose A creates a message pipe,
  // sending one end to B and the other to C. For some brief period of time,
  // messages sent by B or C over this pipe may be proxied through A.
  //
  // If A is suddenly terminated, there may be no way for B's messages to reach
  // C (and vice versa), since the message pipe state may not have been fully
  // propagated to all concerned processes in the system. As such, both B and C
  // may have no choice but to signal peer closure on their respective ends of
  // the pipe, and thus the pipe may be broken despite a lack of intent by
  // either B or C.
  //
  // This can also happen if A creates a pipe and passes one end to B, who then
  // passes it along to C. B may temporarily proxy messages for this pipe
  // between A and C, and B's sudden demise will in turn beget the pipe's
  // own sudden demise.
  //
  // In situations where these sort of arrangements may occur, potentially
  // proxying processes must ensure they are shut down cleanly in order to avoid
  // flaky system behavior.
  //
  // WHEN TO USE FAST SHUTDOWN
  // =========================
  //
  // As a general rule of thumb, if your process never creates a message pipe
  // where both ends are passed to other processes, or never forwards a pipe
  // endpoint from one process to another, fast shutdown is safe. Satisfaction
  // of these constraints can be difficult to prove though, so clean shutdown is
  // a safe default choice.
  //
  // Content renderer processes are a good example of a case where fast shutdown
  // is safe, because as a matter of security and stability, a renderer cannot
  // be trusted to do any proxying on behalf of two other processes anyway.
  //
  // There are other practical scenarios where fast shutdown is safe even if
  // the process may have live proxies. For example, content's browser process
  // is treated as a sort of root process in the system, in the sense that if
  // the browser is terminated, no other part of the system is expected to
  // continue normal operation anyway. In this case the side-effects of fast
  // shutdown are irrelevant, so fast shutdown is preferred.
  enum class ShutdownPolicy {
    // Clean shutdown. This causes the ScopedIPCSupport destructor to *block*
    // the calling thread until clean shutdown is complete. See explanation
    // above for details.
    CLEAN,

    // Fast shutdown. In this case a cheap best-effort attempt is made to
    // shut down the IPC system, but no effort is made to wait for its
    // completion. See explanation above for details.
    FAST,
  };

  ScopedIPCSupport(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
      ShutdownPolicy shutdown_policy);

  ScopedIPCSupport(const ScopedIPCSupport&) = delete;
  ScopedIPCSupport& operator=(const ScopedIPCSupport&) = delete;

  ~ScopedIPCSupport();

 private:
  const ShutdownPolicy shutdown_policy_;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_EMBEDDER_SCOPED_IPC_SUPPORT_H_
