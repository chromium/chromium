// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/socket/udp_node.h"

#include <errno.h>
#include <string.h>

#include <algorithm>

#include "nacl_io/log.h"
#include "nacl_io/pepper_interface.h"
#include "nacl_io/socket/packet.h"
#include "nacl_io/socket/udp_event_emitter.h"
#include "nacl_io/stream/stream_fs.h"

namespace {
const size_t kMaxPacketSize = 65536;
const size_t kDefaultFifoSize = kMaxPacketSize * 8;
}

namespace nacl_io {

class UdpWork : public StreamFs::Work {
 public:
  explicit UdpWork(const ScopedUdpEventEmitter& emitter)
      : StreamFs::Work(emitter->stream()->stream()),
        emitter_(emitter),
        packet_(NULL) {}

  ~UdpWork() { delete packet_; }

  UDPSocketInterface* UDPInterface() {
    return filesystem()->ppapi()->GetUDPSocketInterface();
  }

 protected:
  ScopedUdpEventEmitter emitter_;
  Packet* packet_;
};

class UdpSendWork : public UdpWork {
 public:
  explicit UdpSendWork(const ScopedUdpEventEmitter& emitter,
                       const ScopedSocketNode& node)
      : UdpWork(emitter), node_(node) {}

  virtual bool Start(int32_t val) {
    AUTO_LOCK(emitter_->GetLock());

    // Does the stream exist, and can it send?
    if (!node_->TestStreamFlags(SSF_CAN_SEND))
      return false;

    // Check if we are already sending.
    if (node_->TestStreamFlags(SSF_SENDING))
      return false;

    // If this is a retry packet, packet_ will be already set
    // and we don't need to dequeue from emitter_.
    if (NULL == packet_) {
      packet_ = emitter_->ReadTXPacket_Locked();
      if (NULL == packet_)
        return false;
    }

    int err = UDPInterface()->SendTo(node_->socket_resource(),
                                     packet_->buffer(),
                                     packet_->len(),
                                     packet_->addr(),
                                     filesystem()->GetRunCompletion(this));
    if (err != PP_OK_COMPLETIONPENDING) {
      // Anything else, we should assume the socket has gone bad.
      node_->SetError_Locked(err);
      return false;
    }

    node_->SetStreamFlags(SSF_SENDING);
    return true;
  }

  virtual void Run(int32_t length_error) {
    AUTO_LOCK(emitter_->GetLock());

    if (length_error < 0) {
      if (length_error == PP_ERROR_INPROGRESS) {
        // We need to retry this packet later.
        node_->ClearStreamFlags(SSF_SENDING);
        node_->stream()->EnqueueWork(this);
        return;
      }
      node_->SetError_Locked(length_error);
      return;
    }

    // If we did send, then Q more work.
    node_->ClearStreamFlags(SSF_SENDING);
    node_->QueueOutput();
  }

 private:
  // We assume that transmits will always complete.  If the upstream
  // actually back pressures, enough to prevent the Send callback
  // from triggering, this resource may never go away.
  ScopedSocketNode node_;
};

class UdpRecvWork : public UdpWork {
 public:
  explicit UdpRecvWork(const ScopedUdpEventEmitter& emitter)
      : UdpWork(emitter) {
  }

  virtual bool Start(int32_t val) {
    AUTO_LOCK(emitter_->GetLock());
    UdpNode* stream = static_cast<UdpNode*>(emitter_->stream());

    // Does the stream exist, and can it recv?
    if (NULL == stream || !stream->TestStreamFlags(SSF_CAN_RECV))
      return false;

    // Check if we are already receiving.
    if (stream->TestStreamFlags(SSF_RECVING))
      return false;

    stream->SetStreamFlags(SSF_RECVING);
    int err = UDPInterface()->RecvFrom(stream->socket_resource(),
                                       data_,
                                       kMaxPacketSize,
                                       &addr_,
                                       filesystem()->GetRunCompletion(this));
    if (err != PP_OK_COMPLETIONPENDING) {
      stream->SetError_Locked(err);
      return false;
    }

    return true;
  }

  virtual void Run(int32_t length_error) {
    AUTO_LOCK(emitter_->GetLock());
    UdpNode* stream = static_cast<UdpNode*>(emitter_->stream());
    if (NULL == stream)
      return;

    // On successful receive we queue more input
    if (length_error > 0) {
      Packet* packet = new Packet(filesystem()->ppapi());
      packet->Copy(data_, length_error, addr_);
      filesystem()->ppapi()->ReleaseResource(addr_);
      emitter_->WriteRXPacket_Locked(packet);
      stream->ClearStreamFlags(SSF_RECVING);
      stream->QueueInput();
    } else {
      stream->SetError_Locked(length_error);
    }
  }

 private:
  char data_[kMaxPacketSize];
  PP_Resource addr_;
};

UdpNode::UdpNode(Filesystem* filesystem)
    : SocketNode(SOCK_DGRAM, filesystem),
      emitter_(new UdpEventEmitter(kDefaultFifoSize, kDefaultFifoSize)) {
  emitter_->AttachStream(this);
}

void UdpNode::Destroy() {
  emitter_->DetachStream();
  SocketNode::Destroy();
}

UdpEventEmitter* UdpNode::GetEventEmitter() {
  return emitter_.get();
}

Error UdpNode::Init(int open_flags) {
  Error err = SocketNode::Init(open_flags);
  if (err != 0)
    return err;

  if (UDPInterface() == NULL) {
    LOG_ERROR("Got NULL interface: UDP");
    return EACCES;
  }

  socket_resource_ =
      UDPInterface()->Create(filesystem_->ppapi()->GetInstance());
  if (0 == socket_resource_) {
    LOG_ERROR("Unable to create UDP resource.");
    return EACCES;
  }

  return 0;
}

void UdpNode::QueueInput() {
  UdpRecvWork* work = new UdpRecvWork(emitter_);
  stream()->EnqueueWork(work);
}

void UdpNode::QueueOutput() {
  if (!TestStreamFlags(SSF_CAN_SEND))
    return;

  if (TestStreamFlags(SSF_SENDING))
    return;

  UdpSendWork* work = new UdpSendWork(emitter_, ScopedSocketNode(this));
  stream()->EnqueueWork(work);
}

Error UdpNode::Bind(const struct sockaddr* addr, socklen_t len) {
  if (0 == socket_resource_)
    return EBADF;

  /* Only bind once. */
  if (IsBound())
    return EINVAL;

  PP_Resource out_addr = SockAddrToResource(addr, len);
  if (0 == out_addr)
    return EINVAL;

  int err =
      UDPInterface()->Bind(socket_resource_, out_addr, PP_BlockUntilComplete());
  filesystem_->ppapi()->ReleaseResource(out_addr);
  if (err != 0)
    return PPERROR_TO_ERRNO(err);

  // Get the address that was actually bound (in case addr was 0.0.0.0:0).
  out_addr = UDPInterface()->GetBoundAddress(socket_resource_);
  if (out_addr == 0)
    return EINVAL;

  // Now that we are bound, we can start sending and receiving.
  SetStreamFlags(SSF_CAN_SEND | SSF_CAN_RECV);
  QueueInput();

  local_addr_ = out_addr;
  return 0;
}

Error UdpNode::Connect(const HandleAttr& attr,
                       const struct sockaddr* addr,
                       socklen_t len) {
  if (0 == socket_resource_)
    return EBADF;

  /* Connect for UDP is the default dest, it's legal to change it. */
  if (remote_addr_ != 0) {
    filesystem_->ppapi()->ReleaseResource(remote_addr_);
    remote_addr_ = 0;
  }

  remote_addr_ = SockAddrToResource(addr, len);
  if (0 == remote_addr_)
    return EINVAL;

  return 0;
}

Error UdpNode::Recv_Locked(void* buf,
                           size_t len,
                           PP_Resource* out_addr,
                           int* out_len) {
  Packet* packet = emitter_->ReadRXPacket_Locked();
  *out_len = 0;
  *out_addr = 0;

  if (packet) {
    int capped_len = static_cast<int32_t>(std::min<int>(len, packet->len()));
    memcpy(buf, packet->buffer(), capped_len);

    if (packet->addr() != 0) {
      filesystem_->ppapi()->AddRefResource(packet->addr());
      *out_addr = packet->addr();
    }

    *out_len = capped_len;
    delete packet;
    return 0;
  }

  // Should never happen, Recv_Locked should not be called
  // unless already in a POLLIN state.
  return EBADF;
}

Error UdpNode::Send_Locked(const void* buf,
                           size_t len,
                           PP_Resource addr,
                           int* out_len) {
  if (!IsBound()) {
    // Pepper requires a socket to be bound before it can send.
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    memset(&addr.sin_addr, 0, sizeof(addr.sin_addr));
    Error err =
        Bind(reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr));
    if (err != 0)
      return err;
  }

  *out_len = 0;
  int capped_len = static_cast<int32_t>(std::min<int>(len, kMaxPacketSize));
  Packet* packet = new Packet(filesystem_->ppapi());
  packet->Copy(buf, capped_len, addr);

  emitter_->WriteTXPacket_Locked(packet);
  *out_len = capped_len;
  return 0;
}

Error UdpNode::SetSockOptSocket(int optname,
                                const void* optval,
                                socklen_t len) {
  if (static_cast<size_t>(len) < sizeof(int))
    return EINVAL;

  switch (optname) {
    case SO_RCVBUF: {
      int bufsize = *static_cast<const int*>(optval);
      int32_t error = UDPInterface()->SetOption(
          socket_resource_, PP_UDPSOCKET_OPTION_RECV_BUFFER_SIZE,
          PP_MakeInt32(bufsize), PP_BlockUntilComplete());
      return PPERROR_TO_ERRNO(error);
    }
    case SO_SNDBUF: {
      int bufsize = *static_cast<const int*>(optval);
      int32_t error = UDPInterface()->SetOption(
          socket_resource_, PP_UDPSOCKET_OPTION_SEND_BUFFER_SIZE,
          PP_MakeInt32(bufsize), PP_BlockUntilComplete());
      return PPERROR_TO_ERRNO(error);
    }
    case SO_BROADCAST: {
      int broadcast = *static_cast<const int*>(optval);
      int32_t error = UDPInterface()->SetOption(
          socket_resource_, PP_UDPSOCKET_OPTION_BROADCAST,
          PP_MakeBool(broadcast ? PP_TRUE : PP_FALSE), PP_BlockUntilComplete());
      return PPERROR_TO_ERRNO(error);
    }
    default: { break; }
  }

  return SocketNode::SetSockOptSocket(optname, optval, len);
}

Error UdpNode::SetSockOptIP(int optname, const void* optval, socklen_t len) {
  if (static_cast<size_t>(len) < sizeof(int))
    return EINVAL;

  switch (optname) {
    case IP_MULTICAST_LOOP: {
      int loop = *static_cast<const int*>(optval);
      int32_t error = UDPInterface()->SetOption(
          socket_resource_, PP_UDPSOCKET_OPTION_MULTICAST_LOOP,
          PP_MakeBool(loop ? PP_TRUE : PP_FALSE), PP_BlockUntilComplete());
      return PPERROR_TO_ERRNO(error);
    }
    case IP_MULTICAST_TTL: {
      unsigned int ttl = *static_cast<const unsigned int*>(optval);
      int32_t error = UDPInterface()->SetOption(
          socket_resource_, PP_UDPSOCKET_OPTION_MULTICAST_TTL,
          PP_MakeInt32(ttl), PP_BlockUntilComplete());
      return PPERROR_TO_ERRNO(error);
    }
    case IP_MULTICAST_IF: {
      // PPAPI does not expose this option, but we pretend to support it.
      // TODO(etrunko): store value to be returned by GetSockOpt() when it is
      // implemented.
      return 0;
    }
    case IP_ADD_MEMBERSHIP: {
      const struct ip_mreq* mreq = static_cast<const struct ip_mreq*>(optval);
      struct sockaddr_in sin = {0};
      sin.sin_family = AF_INET;
      memcpy(&sin.sin_addr, &mreq->imr_multiaddr, sizeof(struct in_addr));

      PP_Resource net_addr = SockAddrInToResource(&sin, sizeof(sin));
      int32_t error = UDPInterface()->JoinGroup(socket_resource_, net_addr,
                                                PP_BlockUntilComplete());
      return PPERROR_TO_ERRNO(error);
    }
    case IP_DROP_MEMBERSHIP: {
      const struct ip_mreq* mreq = static_cast<const struct ip_mreq*>(optval);
      struct sockaddr_in sin = {0};
      sin.sin_family = AF_INET;
      memcpy(&sin.sin_addr, &mreq->imr_multiaddr, sizeof(struct in_addr));

      PP_Resource net_addr = SockAddrInToResource(&sin, sizeof(sin));
      int32_t error = UDPInterface()->LeaveGroup(socket_resource_, net_addr,
                                                 PP_BlockUntilComplete());
      return PPERROR_TO_ERRNO(error);
    }
    default: { break; }
  }

  return SocketNode::SetSockOptIP(optname, optval, len);
}

Error UdpNode::SetSockOptIPV6(int optname, const void* optval, socklen_t len) {
  if (static_cast<size_t>(len) < sizeof(int))
    return EINVAL;

  switch (optname) {
    case IPV6_MULTICAST_LOOP: {
      int loop = *static_cast<const int*>(optval);
      int32_t error = UDPInterface()->SetOption(
          socket_resource_, PP_UDPSOCKET_OPTION_MULTICAST_LOOP,
          PP_MakeBool(loop ? PP_TRUE : PP_FALSE), PP_BlockUntilComplete());
      return PPERROR_TO_ERRNO(error);
    }
    case IPV6_MULTICAST_HOPS: {
      unsigned int ttl = *static_cast<const unsigned int*>(optval);
      int32_t error = UDPInterface()->SetOption(
          socket_resource_, PP_UDPSOCKET_OPTION_MULTICAST_TTL,
          PP_MakeInt32(ttl), PP_BlockUntilComplete());
      return PPERROR_TO_ERRNO(error);
    }
    case IPV6_MULTICAST_IF: {
      // PPAPI does not expose this option, but we pretend to support it.
      // TODO(etrunko): store value to be returned by GetSockOpt() when it is
      // implemented.
      return 0;
    }
    case IPV6_JOIN_GROUP: {
      const struct ipv6_mreq* mreq =
          static_cast<const struct ipv6_mreq*>(optval);
      struct sockaddr_in6 sin = {0};
      sin.sin6_family = AF_INET6;
      memcpy(&sin.sin6_addr, &mreq->ipv6mr_multiaddr, sizeof(struct in6_addr));

      PP_Resource net_addr = SockAddrIn6ToResource(&sin, sizeof(sin));
      int32_t error = UDPInterface()->JoinGroup(socket_resource_, net_addr,
                                                PP_BlockUntilComplete());
      return PPERROR_TO_ERRNO(error);
    }
    case IPV6_LEAVE_GROUP: {
      const struct ipv6_mreq* mreq =
          static_cast<const struct ipv6_mreq*>(optval);
      struct sockaddr_in6 sin = {0};
      sin.sin6_family = AF_INET6;
      memcpy(&sin.sin6_addr, &mreq->ipv6mr_multiaddr, sizeof(struct in6_addr));

      PP_Resource net_addr = SockAddrIn6ToResource(&sin, sizeof(sin));
      int32_t error = UDPInterface()->LeaveGroup(socket_resource_, net_addr,
                                                 PP_BlockUntilComplete());
      return PPERROR_TO_ERRNO(error);
    }
    default: { break; }
  }

  return SocketNode::SetSockOptIPV6(optname, optval, len);
}

}  // namespace nacl_io
