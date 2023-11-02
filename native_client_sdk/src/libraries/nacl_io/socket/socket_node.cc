// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/socket/socket_node.h"

#include "nacl_io/ossocket.h"
#ifdef PROVIDES_SOCKET_API

#include <errno.h>
#include <string.h>

#include "nacl_io/filesystem.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/pepper_interface.h"

#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_net_address.h"

namespace nacl_io {

SocketNode::SocketNode(int type, Filesystem* filesystem)
    : StreamNode(filesystem),
      socket_resource_(0),
      local_addr_(0),
      remote_addr_(0),
      socket_flags_(0),
      last_errno_(0),
      keep_alive_(false),
      so_type_(type) {
  memset(&linger_, 0, sizeof(linger_));
  SetType(S_IFSOCK);
}

SocketNode::SocketNode(int type, Filesystem* filesystem, PP_Resource socket)
    : StreamNode(filesystem),
      socket_resource_(socket),
      local_addr_(0),
      remote_addr_(0),
      socket_flags_(0),
      last_errno_(0),
      keep_alive_(false),
      so_type_(type) {
  memset(&linger_, 0, sizeof(linger_));
  SetType(S_IFSOCK);
  filesystem_->ppapi()->AddRefResource(socket_resource_);
}

void SocketNode::Destroy() {
  if (socket_resource_)
    filesystem_->ppapi()->ReleaseResource(socket_resource_);
  if (local_addr_)
    filesystem_->ppapi()->ReleaseResource(local_addr_);
  if (remote_addr_)
    filesystem_->ppapi()->ReleaseResource(remote_addr_);

  socket_resource_ = 0;
  local_addr_ = 0;
  remote_addr_ = 0;
}

// Assume that |addr| and |out_addr| are non-NULL.
Error SocketNode::MMap(void* addr,
                       size_t length,
                       int prot,
                       int flags,
                       size_t offset,
                       void** out_addr) {
  return EACCES;
}

// Normal read/write operations on a Socket are equivalent to
// send/recv with a flag value of 0.
Error SocketNode::Read(const HandleAttr& attr,
                       void* buf,
                       size_t count,
                       int* out_bytes) {
  return Recv(attr, buf, count, 0, out_bytes);
}

Error SocketNode::Write(const HandleAttr& attr,
                        const void* buf,
                        size_t count,
                        int* out_bytes) {
  return Send(attr, buf, count, 0, out_bytes);
}

NetAddressInterface* SocketNode::NetInterface() {
  if (filesystem_->ppapi() == NULL)
    return NULL;

  return filesystem_->ppapi()->GetNetAddressInterface();
}

TCPSocketInterface* SocketNode::TCPInterface() {
  if (filesystem_->ppapi() == NULL)
    return NULL;

  return filesystem_->ppapi()->GetTCPSocketInterface();
}

UDPSocketInterface* SocketNode::UDPInterface() {
  if (filesystem_->ppapi() == NULL)
    return NULL;

  return filesystem_->ppapi()->GetUDPSocketInterface();
}

PP_Resource SocketNode::SockAddrToResource(const struct sockaddr* addr,
                                           socklen_t len) {
  if (NULL == addr)
    return 0;

  if (AF_INET == addr->sa_family) {
    const sockaddr_in* sin = reinterpret_cast<const sockaddr_in*>(addr);
    return SockAddrInToResource(sin, len);
  }

  if (AF_INET6 == addr->sa_family) {
    const sockaddr_in6* sin = reinterpret_cast<const sockaddr_in6*>(addr);
    return SockAddrIn6ToResource(sin, len);
  }
  return 0;
}

PP_Resource SocketNode::SockAddrInToResource(const sockaddr_in* sin,
                                             socklen_t len) {
  if (len != sizeof(sockaddr_in))
    return 0;

  PP_NetAddress_IPv4 addr4;
  memset(&addr4, 0, sizeof(addr4));

  addr4.port = sin->sin_port;
  memcpy(addr4.addr, &sin->sin_addr, sizeof(addr4.addr));
  return filesystem_->ppapi()->GetNetAddressInterface()->CreateFromIPv4Address(
      filesystem_->ppapi()->GetInstance(), &addr4);
}

PP_Resource SocketNode::SockAddrIn6ToResource(const sockaddr_in6* sin,
                                              socklen_t len) {
  if (len != sizeof(sockaddr_in6))
    return 0;

  PP_NetAddress_IPv6 addr6;
  memset(&addr6, 0, sizeof(addr6));

  addr6.port = sin->sin6_port;
  memcpy(addr6.addr, &sin->sin6_addr, sizeof(addr6.addr));
  return filesystem_->ppapi()->GetNetAddressInterface()->CreateFromIPv6Address(
      filesystem_->ppapi()->GetInstance(), &addr6);
}

socklen_t SocketNode::ResourceToSockAddr(PP_Resource addr,
                                         socklen_t len,
                                         struct sockaddr* out_addr) {
  if (0 == addr)
    return 0;

  PP_NetAddress_IPv4 ipv4;
  PP_NetAddress_IPv6 ipv6;

  if (PP_TRUE == NetInterface()->DescribeAsIPv4Address(addr, &ipv4)) {
    sockaddr_in addr4;
    addr4.sin_family = AF_INET;
    addr4.sin_port = ipv4.port;
    memcpy(&addr4.sin_addr, ipv4.addr, sizeof(ipv4.addr));
    memcpy(out_addr, &addr4,
           std::min(len, static_cast<socklen_t>(sizeof(addr4))));

    // Returns required size not copied size like getpeername/getsockname.
    return sizeof(addr4);
  }

  if (PP_TRUE == NetInterface()->DescribeAsIPv6Address(addr, &ipv6)) {
    sockaddr_in6 addr6;
    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = ipv6.port;
    memcpy(&addr6.sin6_addr, ipv6.addr, sizeof(ipv6.addr));
    memcpy(out_addr, &addr6,
           std::min(len, static_cast<socklen_t>(sizeof(addr6))));

    // Returns required size not copied size like getpeername/getsockname.
    return sizeof(addr6);
  }

  return 0;
}

bool SocketNode::IsEquivalentAddress(PP_Resource addr1, PP_Resource addr2) {
  if (addr1 == addr2)
    return true;

  char data1[sizeof(sockaddr_in6)];
  char data2[sizeof(sockaddr_in6)];

  sockaddr* saddr1 = reinterpret_cast<sockaddr*>(data1);
  sockaddr* saddr2 = reinterpret_cast<sockaddr*>(data2);

  socklen_t len1 = ResourceToSockAddr(addr1, sizeof(data1), saddr1);
  socklen_t len2 = ResourceToSockAddr(addr2, sizeof(data2), saddr2);

  if (len1 != len2)
    return false;

  return memcmp(saddr1, saddr2, len1) == 0;
}

Error SocketNode::Accept(const HandleAttr& attr,
                         PP_Resource* new_sock,
                         struct sockaddr* addr,
                         socklen_t* len) {
  return ENOSYS;
}

Error SocketNode::Connect(const HandleAttr& attr,
                          const struct sockaddr* addr,
                          socklen_t len) {
  if (len < 1)
    return EINVAL;

  if (NULL == addr)
    return EFAULT;

  return EOPNOTSUPP;
}

Error SocketNode::Listen(int backlog) {
  return EOPNOTSUPP;
}

Error SocketNode::GetSockOpt(int lvl,
                             int optname,
                             void* optval,
                             socklen_t* len) {
  if (lvl != SOL_SOCKET)
    return ENOPROTOOPT;

  AUTO_LOCK(node_lock_);

  int value = 0;
  socklen_t value_len = 0;
  void* value_ptr = NULL;

  switch (optname) {
    case SO_REUSEADDR:
      // SO_REUSEADDR is effectively always on since we can't
      // disable it with PPAPI sockets.
      value = 1;
      value_ptr = &value;
      value_len = sizeof(value);
      break;
    case SO_TYPE:
      value_ptr = &so_type_;
      value_len = sizeof(so_type_);
      break;
    case SO_LINGER:
      value_ptr = &linger_;
      value_len = sizeof(linger_);
      break;
    case SO_KEEPALIVE:
      value = keep_alive_;
      value_ptr = &value;
      value_len = sizeof(value);
      break;
    case SO_ERROR:
      value_ptr = &last_errno_;
      value_len = sizeof(last_errno_);
      last_errno_ = 0;
      break;
    default:
      return ENOPROTOOPT;
  }

  int copy_bytes = std::min(value_len, *len);
  memcpy(optval, value_ptr, copy_bytes);
  *len = value_len;
  return 0;
}

Error SocketNode::SetSockOpt(int lvl,
                             int optname,
                             const void* optval,
                             socklen_t len) {
  AUTO_LOCK(node_lock_);

  switch (lvl) {
    case SOL_SOCKET: {
      return SetSockOptSocket(optname, optval, len);
    }
    case IPPROTO_TCP: {
      return SetSockOptTCP(optname, optval, len);
    }
    case IPPROTO_IP: {
      return SetSockOptIP(optname, optval, len);
    }
    case IPPROTO_IPV6: {
      return SetSockOptIPV6(optname, optval, len);
    }
    default: { break; }
  }

  return ENOPROTOOPT;
}

Error SocketNode::SetSockOptSocket(int optname,
                                   const void* optval,
                                   socklen_t len) {
  size_t buflen = static_cast<size_t>(len);
  switch (optname) {
    case SO_REUSEADDR: {
      // SO_REUSEADDR is effectivly always on since we can't
      // disable it with PPAPI sockets. Just return success
      // here regardless.
      if (buflen < sizeof(int))
        return EINVAL;
      return 0;
    }
    case SO_LINGER: {
      // Not supported by the PPAPI interface but we preserve
      // the settings and pretend to support it.
      if (buflen < sizeof(struct linger))
        return EINVAL;
      struct linger new_linger = *static_cast<const linger*>(optval);
      // Don't allow setting linger to be enabled until we
      // implement the required synchronous shutdown()/close().
      // TODO(sbc): remove this after http://crbug.com/312401
      // gets fixed.
      if (new_linger.l_onoff != 0)
        return EINVAL;
      linger_ = new_linger;
      return 0;
    }
    case SO_KEEPALIVE: {
      // Not supported by the PPAPI interface but we preserve
      // the flag and pretend to support it.
      if (buflen < sizeof(int))
        return EINVAL;
      int value = *static_cast<const int*>(optval);
      keep_alive_ = value != 0;
      return 0;
    }
  }

  return ENOPROTOOPT;
}

Error SocketNode::SetSockOptTCP(int optname,
                                const void* optval,
                                socklen_t len) {
  return ENOPROTOOPT;
}

Error SocketNode::SetSockOptIP(int optname, const void* optval, socklen_t len) {
  return ENOPROTOOPT;
}

Error SocketNode::SetSockOptIPV6(int optname,
                                 const void* optval,
                                 socklen_t len) {
  return ENOPROTOOPT;
}

Error SocketNode::Bind(const struct sockaddr* addr, socklen_t len) {
  return EINVAL;
}

Error SocketNode::Recv(const HandleAttr& attr,
                       void* buf,
                       size_t len,
                       int flags,
                       int* out_len) {
  return RecvFrom(attr, buf, len, flags, NULL, 0, out_len);
}

Error SocketNode::RecvFrom(const HandleAttr& attr,
                           void* buf,
                           size_t len,
                           int flags,
                           struct sockaddr* src_addr,
                           socklen_t* addrlen,
                           int* out_len) {
  PP_Resource addr = 0;
  if (0 == socket_resource_)
    return EBADF;
  Error err = RecvHelper(attr, buf, len, flags, &addr, out_len);
  if (0 == err && 0 != addr) {
    if (src_addr)
      *addrlen = ResourceToSockAddr(addr, *addrlen, src_addr);

    filesystem_->ppapi()->ReleaseResource(addr);
  }

  return err;
}

Error SocketNode::RecvHelper(const HandleAttr& attr,
                             void* buf,
                             size_t len,
                             int flags,
                             PP_Resource* addr,
                             int* out_len) {
  int ms = read_timeout_;
  if ((flags & MSG_DONTWAIT) || !attr.IsBlocking())
    ms = 0;

  // TODO(bradnelson) BUG=295177
  // For UDP we should support filtering packets when using connect
  EventListenerLock wait(GetEventEmitter());
  Error err = wait.WaitOnEvent(POLLIN, ms);

  // Timeout is treated as a would block for sockets.
  if (ETIMEDOUT == err)
    return EWOULDBLOCK;

  if (err)
    return err;

  err = Recv_Locked(buf, len, addr, out_len);

  // We must have read from then inputbuffer, so Q up some receive work.
  if ((err == 0) && *out_len)
    QueueInput();
  return err;
}

Error SocketNode::Send(const HandleAttr& attr,
                       const void* buf,
                       size_t len,
                       int flags,
                       int* out_len) {
  if (0 == socket_resource_)
    return EBADF;

  if (0 == remote_addr_)
    return ENOTCONN;
  return SendHelper(attr, buf, len, flags, remote_addr_, out_len);
}

Error SocketNode::SendTo(const HandleAttr& attr,
                         const void* buf,
                         size_t len,
                         int flags,
                         const struct sockaddr* dest_addr,
                         socklen_t addrlen,
                         int* out_len) {
  if ((NULL == dest_addr) && (0 == remote_addr_))
    return EDESTADDRREQ;

  PP_Resource addr = SockAddrToResource(dest_addr, addrlen);
  if (0 == addr)
    return EINVAL;

  if (0 == socket_resource_)
    return EBADF;

  Error err = SendHelper(attr, buf, len, flags, addr, out_len);
  filesystem_->ppapi()->ReleaseResource(addr);
  return err;
}

Error SocketNode::SendHelper(const HandleAttr& attr,
                             const void* buf,
                             size_t len,
                             int flags,
                             PP_Resource addr,
                             int* out_len) {
  int ms = write_timeout_;
  if ((flags & MSG_DONTWAIT) || !attr.IsBlocking())
    ms = 0;

  EventListenerLock wait(GetEventEmitter());
  Error err = wait.WaitOnEvent(POLLOUT, ms);

  // Timeout is treated as a would block for sockets.
  if (ETIMEDOUT == err)
    return EWOULDBLOCK;

  if (err)
    return err;

  err = Send_Locked(buf, len, addr, out_len);

  // We must have added to the output buffer, so Q up some transmit work.
  if ((err == 0) && *out_len)
    QueueOutput();
  return err;
}

void SocketNode::SetError_Locked(int pp_error_num) {
  SetStreamFlags(SSF_ERROR | SSF_CLOSED);
  ClearStreamFlags(SSF_CAN_SEND | SSF_CAN_RECV);
  last_errno_ = PPERROR_TO_ERRNO(pp_error_num);
}

Error SocketNode::Shutdown(int how) {
  return EOPNOTSUPP;
}

Error SocketNode::GetPeerName(struct sockaddr* addr, socklen_t* len) {
  if (NULL == addr || NULL == len)
    return EFAULT;

  AUTO_LOCK(node_lock_);
  if (remote_addr_ != 0) {
    *len = ResourceToSockAddr(remote_addr_, *len, addr);
    return 0;
  }

  return ENOTCONN;
}

Error SocketNode::GetSockName(struct sockaddr* addr, socklen_t* len) {
  if (NULL == addr || NULL == len)
    return EFAULT;

  AUTO_LOCK(node_lock_);
  if (local_addr_ == 0) {
    // getsockname succeeds even if the socket is not bound. In this case,
    // just return address 0, port 0.
    memset(addr, 0, *len);
    return 0;
  }

  *len = ResourceToSockAddr(local_addr_, *len, addr);
  return 0;
}

}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API
