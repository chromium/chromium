// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/platform/socket_utils_posix.h"

#include <stddef.h>
#include <sys/socket.h>
#include <unistd.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_NACL)
#include <sys/uio.h>
#endif

namespace mojo {

namespace {

#if !BUILDFLAG(IS_NACL)
bool IsRecoverableError() {
  return errno == ECONNABORTED || errno == EMFILE || errno == ENFILE ||
         errno == ENOMEM || errno == ENOBUFS;
}

bool GetPeerEuid(base::PlatformFile fd, uid_t* peer_euid) {
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OPENBSD) || BUILDFLAG(IS_FREEBSD)
  uid_t socket_euid;
  gid_t socket_gid;
  if (getpeereid(fd, &socket_euid, &socket_gid) < 0) {
    PLOG(ERROR) << "getpeereid " << fd;
    return false;
  }
  *peer_euid = socket_euid;
  return true;
#else
  struct ucred cred;
  socklen_t cred_len = sizeof(cred);
  if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &cred_len) < 0) {
    PLOG(ERROR) << "getsockopt " << fd;
    return false;
  }
  if (static_cast<unsigned>(cred_len) < sizeof(cred)) {
    NOTREACHED() << "Truncated ucred from SO_PEERCRED?";
  }
  *peer_euid = cred.uid;
  return true;
#endif
}

bool IsPeerAuthorized(base::PlatformFile fd) {
  uid_t peer_euid;
  if (!GetPeerEuid(fd, &peer_euid))
    return false;
  if (peer_euid != geteuid()) {
    DLOG(ERROR) << "Client euid is not authorized";
    return false;
  }
  return true;
}
#endif  // !BUILDFLAG(IS_NACL)

// NOTE: On Linux |SIGPIPE| is suppressed by passing |MSG_NOSIGNAL| to
// |sendmsg()|. On Mac we instead set |SO_NOSIGPIPE| on the socket itself.
#if BUILDFLAG(IS_APPLE)
constexpr int kSendmsgFlags = 0;
#else
constexpr int kSendmsgFlags = MSG_NOSIGNAL;
#endif

}  // namespace

ssize_t SocketWrite(base::PlatformFile socket,
                    const void* bytes,
                    size_t num_bytes) {
#if BUILDFLAG(IS_APPLE)
  return HANDLE_EINTR(write(socket, bytes, num_bytes));
#else
  return send(socket, bytes, num_bytes, kSendmsgFlags);
#endif
}

ssize_t SocketWritev(base::PlatformFile socket,
                     struct iovec* iov,
                     size_t num_iov) {
#if BUILDFLAG(IS_APPLE)
  return HANDLE_EINTR(writev(socket, iov, static_cast<int>(num_iov)));
#else
  struct msghdr msg = {};
  msg.msg_iov = iov;
  msg.msg_iovlen = num_iov;
  return HANDLE_EINTR(sendmsg(socket, &msg, kSendmsgFlags));
#endif
}

ssize_t SendmsgWithHandles(base::PlatformFile socket,
                           struct iovec* iov,
                           size_t num_iov,
                           const std::vector<base::ScopedFD>& descriptors) {
  DCHECK(iov);
  DCHECK_GT(num_iov, 0u);
  DCHECK(!descriptors.empty());
  CHECK_LE(descriptors.size(), kMaxSendmsgHandles);

  char cmsg_buf[CMSG_SPACE(kMaxSendmsgHandles * sizeof(int))];
  struct msghdr msg = {};
  msg.msg_iov = iov;
  msg.msg_iovlen = num_iov;
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = CMSG_LEN(descriptors.size() * sizeof(int));
  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(descriptors.size() * sizeof(int));
  for (size_t i = 0; i < descriptors.size(); ++i) {
    DCHECK_GE(descriptors[i].get(), 0);
    reinterpret_cast<int*>(CMSG_DATA(cmsg))[i] = descriptors[i].get();
  }
  return HANDLE_EINTR(sendmsg(socket, &msg, kSendmsgFlags));
}

ssize_t SocketRecvmsg(base::PlatformFile socket,
                      void* buf,
                      size_t num_bytes,
                      std::vector<base::ScopedFD>* descriptors,
                      bool block) {
  struct iovec iov = {buf, num_bytes};
  char cmsg_buf[CMSG_SPACE(kMaxSendmsgHandles * sizeof(int))];
  struct msghdr msg = {};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = sizeof(cmsg_buf);
  ssize_t result =
      HANDLE_EINTR(recvmsg(socket, &msg, block ? 0 : MSG_DONTWAIT));
  if (result < 0)
    return result;

  if (msg.msg_controllen == 0)
    return result;

  DCHECK(!(msg.msg_flags & MSG_CTRUNC));

  descriptors->clear();
  for (cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg;
       cmsg = CMSG_NXTHDR(&msg, cmsg)) {
    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
      size_t payload_length = cmsg->cmsg_len - CMSG_LEN(0);
      DCHECK_EQ(payload_length % sizeof(int), 0u);
      size_t num_fds = payload_length / sizeof(int);
      const int* fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
      for (size_t i = 0; i < num_fds; ++i) {
        base::ScopedFD fd(fds[i]);
        DCHECK(fd.is_valid());
        descriptors->emplace_back(std::move(fd));
      }
    }
  }

  return result;
}

bool AcceptSocketConnection(base::PlatformFile server_fd,
                            base::ScopedFD* connection_fd,
                            bool check_peer_user) {
  DCHECK_GE(server_fd, 0);
  connection_fd->reset();
#if BUILDFLAG(IS_NACL)
  NOTREACHED();
#else
  base::ScopedFD accepted_handle(HANDLE_EINTR(accept(server_fd, nullptr, 0)));
  if (!accepted_handle.is_valid())
    return IsRecoverableError();
  if (check_peer_user && !IsPeerAuthorized(accepted_handle.get()))
    return true;
  if (!base::SetNonBlocking(accepted_handle.get())) {
    PLOG(ERROR) << "base::SetNonBlocking() failed " << accepted_handle.get();
    return true;
  }

  *connection_fd = std::move(accepted_handle);
  return true;
#endif  // BUILDFLAG(IS_NACL)
}

}  // namespace mojo
