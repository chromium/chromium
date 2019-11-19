// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sandbox_ipc_linux.h"

#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/linux_util.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/common/content_switches.h"
#include "sandbox/linux/services/libc_interceptor.h"
#include "services/service_manager/sandbox/linux/sandbox_linux.h"

namespace content {

const size_t kMaxSandboxIPCMessagePayloadSize = 64;

// static
SandboxIPCHandler::SandboxIPCHandler(int lifeline_fd, int browser_socket)
    : lifeline_fd_(lifeline_fd), browser_socket_(browser_socket) {}

void SandboxIPCHandler::Run() {
  struct pollfd pfds[2];
  pfds[0].fd = lifeline_fd_;
  pfds[0].events = POLLIN;
  pfds[1].fd = browser_socket_;
  pfds[1].events = POLLIN;

  int failed_polls = 0;
  for (;;) {
    const int r =
        HANDLE_EINTR(poll(pfds, base::size(pfds), -1 /* no timeout */));
    // '0' is not a possible return value with no timeout.
    DCHECK_NE(0, r);
    if (r < 0) {
      PLOG(WARNING) << "poll";
      if (failed_polls++ == 3) {
        LOG(FATAL) << "poll(2) failing. SandboxIPCHandler aborting.";
        return;
      }
      continue;
    }

    failed_polls = 0;

    // The browser process will close the other end of this pipe on shutdown,
    // so we should exit.
    if (pfds[0].revents) {
      break;
    }

    // If poll(2) reports an error condition in this fd,
    // we assume the zygote is gone and we exit the loop.
    if (pfds[1].revents & (POLLERR | POLLHUP)) {
      break;
    }

    if (pfds[1].revents & POLLIN) {
      HandleRequestFromChild(browser_socket_);
    }
  }

  VLOG(1) << "SandboxIPCHandler stopping.";
}

void SandboxIPCHandler::HandleRequestFromChild(int fd) {
  std::vector<base::ScopedFD> fds;

  // A FontConfigIPC::METHOD_MATCH message could be kMaxFontFamilyLength
  // bytes long (this is the largest message type).
  // The size limit  used to be FontConfigIPC::kMaxFontFamilyLength which was
  // 2048, but we do not receive FontConfig IPC here anymore. The only payloads
  // here are service_manager::SandboxLinux::METHOD_MAKE_SHARED_MEMORY_SEGMENT
  // and HandleLocalTime from libc_interceptor for which
  // kMaxSandboxIPCMessagePayloadSize set to 64 should be plenty.
  // 128 bytes padding are necessary so recvmsg() does not return MSG_TRUNC
  // error for a maximum length message.
  char buf[kMaxSandboxIPCMessagePayloadSize + 128];

  const ssize_t len =
      base::UnixDomainSocket::RecvMsg(fd, buf, sizeof(buf), &fds);
  if (len == -1) {
    // TODO: should send an error reply, or the sender might block forever.
    if (errno == EMSGSIZE) {
      NOTREACHED() << "Sandbox host message is larger than "
                      "kMaxSandboxIPCMessagePayloadSize";
    } else {
      PLOG(ERROR) << "Recvmsg failed";
      NOTREACHED();
    }
    return;
  }
  if (fds.empty())
    return;

  base::Pickle pickle(buf, len);
  base::PickleIterator iter(pickle);

  int kind;
  if (!iter.ReadInt(&kind))
    return;

  // Give sandbox first shot at request, if it is not handled, then
  // false is returned and we continue on.
  if (sandbox::HandleInterceptedCall(kind, fd, iter, fds))
    return;

  if (kind ==
      service_manager::SandboxLinux::METHOD_MAKE_SHARED_MEMORY_SEGMENT) {
    HandleMakeSharedMemorySegment(fd, iter, fds);
    return;
  }
  NOTREACHED();
}

void SandboxIPCHandler::HandleMakeSharedMemorySegment(
    int fd,
    base::PickleIterator iter,
    const std::vector<base::ScopedFD>& fds) {
  uint32_t size;
  if (!iter.ReadUInt32(&size))
    return;
  // TODO(crbug.com/982879): executable shared memory should be removed when
  // NaCl is unshipped.
  bool executable;
  if (!iter.ReadBool(&executable))
    return;
  base::ScopedFD shm_fd;
  if (executable) {
    shm_fd =
        base::subtle::PlatformSharedMemoryRegion::ExecutableRegion::CreateFD(
            size);
  } else {
    base::subtle::PlatformSharedMemoryRegion region =
        base::subtle::PlatformSharedMemoryRegion::CreateUnsafe(size);
    shm_fd = std::move(region.PassPlatformHandle().fd);
  }
  base::Pickle reply;
  SendRendererReply(fds, reply, shm_fd.get());
  // shm_fd will close the handle which is no longer needed by this process.
}

void SandboxIPCHandler::SendRendererReply(
    const std::vector<base::ScopedFD>& fds,
    const base::Pickle& reply,
    int reply_fd) {
  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  struct iovec iov = {const_cast<void*>(reply.data()), reply.size()};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char control_buffer[CMSG_SPACE(sizeof(reply_fd))];

  if (reply_fd != -1) {
    struct stat st;
    if (fstat(reply_fd, &st) == 0 && S_ISDIR(st.st_mode)) {
      LOG(FATAL) << "Tried to send a directory descriptor over sandbox IPC";
      // We must never send directory descriptors to a sandboxed process
      // because they can use openat with ".." elements in the path in order
      // to escape the sandbox and reach the real filesystem.
    }

    struct cmsghdr* cmsg;
    msg.msg_control = control_buffer;
    msg.msg_controllen = sizeof(control_buffer);
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(reply_fd));
    memcpy(CMSG_DATA(cmsg), &reply_fd, sizeof(reply_fd));
    msg.msg_controllen = cmsg->cmsg_len;
  }

  if (HANDLE_EINTR(sendmsg(fds[0].get(), &msg, MSG_DONTWAIT)) < 0)
    PLOG(ERROR) << "sendmsg";
}

SandboxIPCHandler::~SandboxIPCHandler() {
  if (IGNORE_EINTR(close(lifeline_fd_)) < 0)
    PLOG(ERROR) << "close";
  if (IGNORE_EINTR(close(browser_socket_)) < 0)
    PLOG(ERROR) << "close";
}

}  // namespace content
