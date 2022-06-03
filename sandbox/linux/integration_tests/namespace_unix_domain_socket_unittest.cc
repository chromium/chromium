// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sched.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/files/scoped_file.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket.h"
#include "base/process/process.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "sandbox/linux/tests/unit_tests.h"

// Additional tests for base's UnixDomainSocket to make sure it behaves
// correctly in the presence of sandboxing functionality (e.g., receiving
// PIDs across namespaces).

namespace sandbox {

namespace {

const char kHello[] = "hello";

// If the calling process isn't root, then try using unshare(CLONE_NEWUSER)
// to fake it.
void FakeRoot() {
  // If we're already root, then allow test to proceed.
  if (geteuid() == 0)
    return;

  // Otherwise hope the kernel supports unprivileged namespaces.
  if (unshare(CLONE_NEWUSER) == 0)
    return;

  printf("Permission to use CLONE_NEWPID missing; skipping test.\n");
  UnitTests::IgnoreThisTest();
}

void WaitForExit(pid_t pid) {
  int status;
  CHECK_EQ(pid, HANDLE_EINTR(waitpid(pid, &status, 0)));
  CHECK(WIFEXITED(status));
  CHECK_EQ(0, WEXITSTATUS(status));
}

base::ProcessId GetParentProcessId(base::ProcessId pid) {
  // base::GetParentProcessId() is defined as taking a ProcessHandle instead of
  // a ProcessId, even though it's a POSIX-only function and IDs and Handles
  // are both simply pid_t on POSIX... :/
  base::Process process = base::Process::Open(pid);
  CHECK(process.IsValid());
  base::ProcessId ret = base::GetParentProcessId(process.Handle());
  return ret;
}

// SendHello sends a "hello" to socket fd, and then blocks until the recipient
// acknowledges it by calling RecvHello.
void SendHello(int fd) {
  int pipe_fds[2];
  CHECK_EQ(0, pipe(pipe_fds));
  base::ScopedFD read_pipe(pipe_fds[0]);
  base::ScopedFD write_pipe(pipe_fds[1]);

  std::vector<int> send_fds;
  send_fds.push_back(write_pipe.get());
  CHECK(base::UnixDomainSocket::SendMsg(fd, kHello, sizeof(kHello), send_fds));

  write_pipe.reset();

  // Block until receiver closes their end of the pipe.
  char ch;
  CHECK_EQ(0, HANDLE_EINTR(read(read_pipe.get(), &ch, 1)));
}

// RecvHello receives and acknowledges a "hello" on socket fd, and returns the
// process ID of the sender in sender_pid.  Optionally, write_pipe can be used
// to return a file descriptor, and the acknowledgement will be delayed until
// the descriptor is closed.
// (Implementation details: SendHello allocates a new pipe, sends us the writing
// end alongside the "hello" message, and then blocks until we close the writing
// end of the pipe.)
void RecvHello(int fd,
               base::ProcessId* sender_pid,
               base::ScopedFD* write_pipe = NULL) {
  // Extra receiving buffer space to make sure we really received only
  // sizeof(kHello) bytes and it wasn't just truncated to fit the buffer.
  char buf[sizeof(kHello) + 1];
  std::vector<base::ScopedFD> message_fds;
  ssize_t n = base::UnixDomainSocket::RecvMsgWithPid(
      fd, buf, sizeof(buf), &message_fds, sender_pid);
  CHECK_EQ(sizeof(kHello), static_cast<size_t>(n));
  CHECK_EQ(0, memcmp(buf, kHello, sizeof(kHello)));
  CHECK_EQ(1U, message_fds.size());
  if (write_pipe)
    std::swap(*write_pipe, message_fds[0]);
}

// Check that receiving PIDs works across a fork().
SANDBOX_TEST(UnixDomainSocketTest, Fork) {
  int fds[2];
  CHECK_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds));
  base::ScopedFD recv_sock(fds[0]);
  base::ScopedFD send_sock(fds[1]);

  CHECK(base::UnixDomainSocket::EnableReceiveProcessId(recv_sock.get()));

  const pid_t pid = fork();
  CHECK_NE(-1, pid);
  if (pid == 0) {
    // Child process.
    recv_sock.reset();
    SendHello(send_sock.get());
    _exit(0);
  }

  // Parent process.
  send_sock.reset();

  base::ProcessId sender_pid;
  RecvHello(recv_sock.get(), &sender_pid);
  CHECK_EQ(pid, sender_pid);

  WaitForExit(pid);
}

// Similar to Fork above, but forking the child into a new pid namespace.
SANDBOX_TEST(UnixDomainSocketTest, Namespace) {
  FakeRoot();

  int fds[2];
  CHECK_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds));
  base::ScopedFD recv_sock(fds[0]);
  base::ScopedFD send_sock(fds[1]);

  CHECK(base::UnixDomainSocket::EnableReceiveProcessId(recv_sock.get()));

  const pid_t pid = sys_clone(CLONE_NEWPID | SIGCHLD, 0, 0, 0, 0);
  CHECK_NE(-1, pid);
  if (pid == 0) {
    // Child process.
    recv_sock.reset();

    // Check that we think we're pid 1 in our new namespace.
    CHECK_EQ(1, sys_getpid());

    SendHello(send_sock.get());
    _exit(0);
  }

  // Parent process.
  send_sock.reset();

  base::ProcessId sender_pid;
  RecvHello(recv_sock.get(), &sender_pid);
  CHECK_EQ(pid, sender_pid);

  WaitForExit(pid);
}

// Again similar to Fork, but now with nested PID namespaces.
SANDBOX_TEST(UnixDomainSocketTest, DoubleNamespace) {
  FakeRoot();

  int fds[2];
  CHECK_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds));
  base::ScopedFD recv_sock(fds[0]);
  base::ScopedFD send_sock(fds[1]);

  CHECK(base::UnixDomainSocket::EnableReceiveProcessId(recv_sock.get()));

  const pid_t pid = sys_clone(CLONE_NEWPID | SIGCHLD, 0, 0, 0, 0);
  CHECK_NE(-1, pid);
  if (pid == 0) {
    // Child process.
    recv_sock.reset();

    const pid_t pid2 = sys_clone(CLONE_NEWPID | SIGCHLD, 0, 0, 0, 0);
    CHECK_NE(-1, pid2);

    if (pid2 != 0) {
      // Wait for grandchild to run to completion; see comments below.
      WaitForExit(pid2);

      // Fallthrough once grandchild has sent its hello and exited.
    }

    // Check that we think we're pid 1.
    CHECK_EQ(1, sys_getpid());

    SendHello(send_sock.get());
    _exit(0);
  }

  // Parent process.
  send_sock.reset();

  // We have two messages to receive: first from the grand-child,
  // then from the child.
  for (unsigned iteration = 0; iteration < 2; ++iteration) {
    base::ProcessId sender_pid;
    base::ScopedFD pipe_fd;
    RecvHello(recv_sock.get(), &sender_pid, &pipe_fd);

    // We need our child and grandchild processes to both be alive for
    // GetParentProcessId() to return a valid pid, hence the pipe trickery.
    // (On the first iteration, grandchild is blocked reading from the pipe
    // until we close it, and child is blocked waiting for grandchild to exit.)
    switch (iteration) {
      case 0:  // Grandchild's message
        // Check that sender_pid refers to our grandchild by checking that pid
        // (our child) is its parent.
        CHECK_EQ(pid, GetParentProcessId(sender_pid));
        break;
      case 1:  // Child's message
        CHECK_EQ(pid, sender_pid);
        break;
      default:
        NOTREACHED();
    }
  }

  WaitForExit(pid);
}

// Tests that GetPeerPid() returns 0 if the peer does not exist in caller's
// namespace.
SANDBOX_TEST(UnixDomainSocketTest, ImpossiblePid) {
  FakeRoot();

  int fds[2];
  CHECK_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds));
  base::ScopedFD send_sock(fds[0]);
  base::ScopedFD recv_sock(fds[1]);

  CHECK(base::UnixDomainSocket::EnableReceiveProcessId(recv_sock.get()));

  const pid_t pid = sys_clone(CLONE_NEWPID | SIGCHLD, 0, 0, 0, 0);
  CHECK_NE(-1, pid);
  if (pid == 0) {
    // Child process.
    send_sock.reset();

    base::ProcessId sender_pid;
    RecvHello(recv_sock.get(), &sender_pid);
    CHECK_EQ(0, sender_pid);
    _exit(0);
  }

  // Parent process.
  recv_sock.reset();
  SendHello(send_sock.get());
  WaitForExit(pid);
}

}  // namespace

}  // namespace sandbox
