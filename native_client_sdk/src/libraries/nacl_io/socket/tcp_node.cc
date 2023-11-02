// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/ossocket.h"
#ifdef PROVIDES_SOCKET_API

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <algorithm>

#include "nacl_io/kernel_handle.h"
#include "nacl_io/log.h"
#include "nacl_io/pepper_interface.h"
#include "nacl_io/socket/tcp_node.h"
#include "nacl_io/stream/stream_fs.h"

namespace {
const size_t kMaxPacketSize = 65536;
const size_t kDefaultFifoSize = kMaxPacketSize * 8;
}

namespace nacl_io {

class TcpWork : public StreamFs::Work {
 public:
  explicit TcpWork(const ScopedTcpEventEmitter& emitter)
      : StreamFs::Work(emitter->stream()->stream()),
        emitter_(emitter),
        data_(NULL) {}

  ~TcpWork() {
    free(data_);
  }

  TCPSocketInterface* TCPInterface() {
    return filesystem()->ppapi()->GetTCPSocketInterface();
  }

 protected:
  ScopedTcpEventEmitter emitter_;
  char* data_;
};

class TcpSendWork : public TcpWork {
 public:
  explicit TcpSendWork(const ScopedTcpEventEmitter& emitter,
                       const ScopedSocketNode& stream)
      : TcpWork(emitter), node_(stream) {}

  virtual bool Start(int32_t val) {
    AUTO_LOCK(emitter_->GetLock());

    // Does the stream exist, and can it send?
    if (!node_->TestStreamFlags(SSF_CAN_SEND))
      return false;

    // Check if we are already sending.
    if (node_->TestStreamFlags(SSF_SENDING))
      return false;

    size_t tx_data_avail = emitter_->BytesInOutputFIFO();
    int capped_len = std::min(tx_data_avail, kMaxPacketSize);
    if (capped_len == 0)
      return false;

    data_ = (char*)malloc(capped_len);
    assert(data_);
    if (data_ == NULL)
      return false;
    emitter_->ReadOut_Locked(data_, capped_len);

    int err = TCPInterface()->Write(node_->socket_resource(),
                                    data_,
                                    capped_len,
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
      // Send failed, mark the socket as bad
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

class TcpRecvWork : public TcpWork {
 public:
  explicit TcpRecvWork(const ScopedTcpEventEmitter& emitter)
      : TcpWork(emitter) {}

  virtual bool Start(int32_t val) {
    AUTO_LOCK(emitter_->GetLock());
    TcpNode* stream = static_cast<TcpNode*>(emitter_->stream());

    // Does the stream exist, and can it recv?
    if (NULL == stream || !stream->TestStreamFlags(SSF_CAN_RECV))
      return false;

    // If we are not currently receiving
    if (stream->TestStreamFlags(SSF_RECVING))
      return false;

    size_t rx_space_avail = emitter_->SpaceInInputFIFO();
    int capped_len =
        static_cast<int32_t>(std::min(rx_space_avail, kMaxPacketSize));

    if (capped_len == 0)
      return false;

    data_ = (char*)malloc(capped_len);
    assert(data_);
    if (data_ == NULL)
      return false;
    int err = TCPInterface()->Read(stream->socket_resource(),
                                   data_,
                                   capped_len,
                                   filesystem()->GetRunCompletion(this));
    if (err != PP_OK_COMPLETIONPENDING) {
      // Anything else, we should assume the socket has gone bad.
      stream->SetError_Locked(err);
      return false;
    }

    stream->SetStreamFlags(SSF_RECVING);
    return true;
  }

  virtual void Run(int32_t length_error) {
    AUTO_LOCK(emitter_->GetLock());
    TcpNode* stream = static_cast<TcpNode*>(emitter_->stream());

    if (!stream)
      return;

    if (length_error < 0) {
      stream->SetError_Locked(length_error);
      return;
    } else if (length_error == 0) {
      stream->SetStreamFlags(SSF_RECV_ENDOFSTREAM);
      emitter_->SetRecvEndOfStream_Locked();
    }

    // If we successfully received, queue more input
    emitter_->WriteIn_Locked(data_, length_error);
    stream->ClearStreamFlags(SSF_RECVING);
    stream->QueueInput();
  }
};

class TCPAcceptWork : public StreamFs::Work {
 public:
  explicit TCPAcceptWork(StreamFs* stream, const ScopedTcpEventEmitter& emitter)
      : StreamFs::Work(stream), emitter_(emitter) {}

  TCPSocketInterface* TCPInterface() {
    return filesystem()->ppapi()->GetTCPSocketInterface();
  }

  virtual bool Start(int32_t val) {
    AUTO_LOCK(emitter_->GetLock());
    TcpNode* node = static_cast<TcpNode*>(emitter_->stream());

    // Does the stream exist, and can it accept?
    if (NULL == node)
      return false;

    // If we are not currently accepting
    if (!node->TestStreamFlags(SSF_LISTENING))
      return false;

    int err = TCPInterface()->Accept(node->socket_resource(),
                                     &new_socket_,
                                     filesystem()->GetRunCompletion(this));

    if (err != PP_OK_COMPLETIONPENDING) {
      // Anything else, we should assume the socket has gone bad.
      node->SetError_Locked(err);
      return false;
    }

    return true;
  }

  virtual void Run(int32_t error) {
    AUTO_LOCK(emitter_->GetLock());
    TcpNode* node = static_cast<TcpNode*>(emitter_->stream());

    if (node == NULL)
      return;

    if (error != PP_OK) {
      node->SetError_Locked(error);
      return;
    }

    emitter_->SetAcceptedSocket_Locked(new_socket_);
  }

 protected:
  PP_Resource new_socket_;
  ScopedTcpEventEmitter emitter_;
};

class TCPConnectWork : public StreamFs::Work {
 public:
  explicit TCPConnectWork(StreamFs* stream,
                          const ScopedTcpEventEmitter& emitter)
      : StreamFs::Work(stream), emitter_(emitter) {}

  TCPSocketInterface* TCPInterface() {
    return filesystem()->ppapi()->GetTCPSocketInterface();
  }

  virtual bool Start(int32_t val) {
    AUTO_LOCK(emitter_->GetLock());
    TcpNode* node = static_cast<TcpNode*>(emitter_->stream());

    // Does the stream exist, and can it connect?
    if (NULL == node)
      return false;

    int err = TCPInterface()->Connect(node->socket_resource(),
                                      node->remote_addr(),
                                      filesystem()->GetRunCompletion(this));
    if (err != PP_OK_COMPLETIONPENDING) {
      // Anything else, we should assume the socket has gone bad.
      node->SetError_Locked(err);
      return false;
    }

    return true;
  }

  virtual void Run(int32_t error) {
    AUTO_LOCK(emitter_->GetLock());
    TcpNode* node = static_cast<TcpNode*>(emitter_->stream());

    if (node == NULL)
      return;

    if (error != PP_OK) {
      node->ConnectFailed_Locked();
      node->SetError_Locked(error);
      return;
    }

    node->ConnectDone_Locked();
  }

 protected:
  ScopedTcpEventEmitter emitter_;
};

TcpNode::TcpNode(Filesystem* filesystem)
    : SocketNode(SOCK_STREAM, filesystem),
      emitter_(new TcpEventEmitter(kDefaultFifoSize, kDefaultFifoSize)),
      tcp_nodelay_(false) {
  emitter_->AttachStream(this);
}

TcpNode::TcpNode(Filesystem* filesystem, PP_Resource socket)
    : SocketNode(SOCK_STREAM, filesystem, socket),
      emitter_(new TcpEventEmitter(kDefaultFifoSize, kDefaultFifoSize)),
      tcp_nodelay_(false) {
  emitter_->AttachStream(this);
}

void TcpNode::Destroy() {
  emitter_->DetachStream();
  SocketNode::Destroy();
}

Error TcpNode::Init(int open_flags) {
  Error err = SocketNode::Init(open_flags);
  if (err != 0)
    return err;

  if (TCPInterface() == NULL) {
    LOG_ERROR("Got NULL interface: TCP");
    return EACCES;
  }

  if (socket_resource_ != 0) {
    // TCP sockets that are contructed with an existing socket_resource_
    // are those that generated from calls to Accept() and therefore are
    // already connected.
    remote_addr_ = TCPInterface()->GetRemoteAddress(socket_resource_);
    ConnectDone_Locked();
  } else {
    socket_resource_ =
        TCPInterface()->Create(filesystem_->ppapi()->GetInstance());
    if (0 == socket_resource_) {
      LOG_ERROR("Unable to create TCP resource.");
      return EACCES;
    }
    SetStreamFlags(SSF_CAN_CONNECT);
  }

  return 0;
}

EventEmitter* TcpNode::GetEventEmitter() {
  return emitter_.get();
}

void TcpNode::SetError_Locked(int pp_error_num) {
  SocketNode::SetError_Locked(pp_error_num);
  emitter_->SetError_Locked();
}

Error TcpNode::GetSockOpt(int lvl, int optname, void* optval, socklen_t* len) {
  if (lvl == IPPROTO_TCP && optname == TCP_NODELAY) {
    AUTO_LOCK(node_lock_);
    int value = tcp_nodelay_;
    socklen_t value_len = static_cast<socklen_t>(sizeof(value));
    int copy_bytes = std::min(value_len, *len);
    memcpy(optval, &value, copy_bytes);
    *len = value_len;
    return 0;
  }

  return SocketNode::GetSockOpt(lvl, optname, optval, len);
}

Error TcpNode::SetNoDelay_Locked() {
  if (!IsConnected())
    return 0;

  int32_t error =
      TCPInterface()->SetOption(socket_resource_,
                                PP_TCPSOCKET_OPTION_NO_DELAY,
                                PP_MakeBool(tcp_nodelay_ ? PP_TRUE : PP_FALSE),
                                PP_BlockUntilComplete());
  return PPERROR_TO_ERRNO(error);
}

Error TcpNode::SetSockOptSocket(int optname,
                                const void* optval,
                                socklen_t len) {
  if (static_cast<size_t>(len) < sizeof(int))
    return EINVAL;
  int bufsize = *static_cast<const int*>(optval);

  if (optname == SO_RCVBUF) {
    int32_t error =
        TCPInterface()->SetOption(socket_resource_,
                                  PP_TCPSOCKET_OPTION_RECV_BUFFER_SIZE,
                                  PP_MakeInt32(bufsize),
                                  PP_BlockUntilComplete());
    return PPERROR_TO_ERRNO(error);
  } else if (optname == SO_SNDBUF) {
    int32_t error = TCPInterface()->SetOption(
        socket_resource_, PP_TCPSOCKET_OPTION_SEND_BUFFER_SIZE,
        PP_MakeInt32(bufsize), PP_BlockUntilComplete());
    return PPERROR_TO_ERRNO(error);
  }

  return SocketNode::SetSockOptSocket(optname, optval, len);
}

Error TcpNode::SetSockOptTCP(int optname, const void* optval, socklen_t len) {
  if (optname == TCP_NODELAY) {
    if (static_cast<size_t>(len) < sizeof(int))
      return EINVAL;
    tcp_nodelay_ = *static_cast<const int*>(optval) != 0;
    return SetNoDelay_Locked();
  }

  return SocketNode::SetSockOptTCP(optname, optval, len);
}

void TcpNode::QueueAccept() {
  StreamFs::Work* work = new TCPAcceptWork(stream(), emitter_);
  stream()->EnqueueWork(work);
}

void TcpNode::QueueConnect() {
  StreamFs::Work* work = new TCPConnectWork(stream(), emitter_);
  stream()->EnqueueWork(work);
}

void TcpNode::QueueInput() {
  if (TestStreamFlags(SSF_RECV_ENDOFSTREAM))
    return;

  StreamFs::Work* work = new TcpRecvWork(emitter_);
  stream()->EnqueueWork(work);
}

void TcpNode::QueueOutput() {
  if (TestStreamFlags(SSF_SENDING))
    return;

  if (!TestStreamFlags(SSF_CAN_SEND))
    return;

  if (0 == emitter_->BytesInOutputFIFO())
    return;

  StreamFs::Work* work = new TcpSendWork(emitter_, ScopedSocketNode(this));
  stream()->EnqueueWork(work);
}

Error TcpNode::Accept(const HandleAttr& attr,
                      PP_Resource* out_sock,
                      struct sockaddr* addr,
                      socklen_t* len) {
  EventListenerLock wait(GetEventEmitter());

  if (!TestStreamFlags(SSF_LISTENING))
    return EINVAL;

  // Either block forever or not at all
  int ms = attr.IsBlocking() ? -1 : 0;

  Error err = wait.WaitOnEvent(POLLIN, ms);
  if (ETIMEDOUT == err)
    return EWOULDBLOCK;

  int s = emitter_->GetAcceptedSocket_Locked();
  // Non-blocking case.
  if (s == 0)
    return EAGAIN;

  // Consume the new socket and start listening for the next one
  *out_sock = s;
  emitter_->ClearEvents_Locked(POLLIN);

  // Set the out paramaters
  if (addr && len) {
    PP_Resource remote_addr = TCPInterface()->GetRemoteAddress(*out_sock);
    *len = ResourceToSockAddr(remote_addr, *len, addr);
    filesystem_->ppapi()->ReleaseResource(remote_addr);
  }

  QueueAccept();
  return 0;
}

// We can not bind a client socket with PPAPI.  For now we ignore the
// bind but report the correct address later, just in case someone is
// binding without really caring what the address is (for example to
// select a more optimized interface/route.)
Error TcpNode::Bind(const struct sockaddr* addr, socklen_t len) {
  AUTO_LOCK(node_lock_);

  /* Only bind once. */
  if (IsBound())
    return EINVAL;

  local_addr_ = SockAddrToResource(addr, len);
  int err = TCPInterface()->Bind(
      socket_resource_, local_addr_, PP_BlockUntilComplete());

  // If we fail, release the local addr resource
  if (err != PP_OK) {
    filesystem_->ppapi()->ReleaseResource(local_addr_);
    local_addr_ = 0;
    return PPERROR_TO_ERRNO(err);
  }

  local_addr_ = TCPInterface()->GetLocalAddress(socket_resource_);
  return 0;
}

Error TcpNode::Connect(const HandleAttr& attr,
                       const struct sockaddr* addr,
                       socklen_t len) {
  EventListenerLock wait(GetEventEmitter());

  if (TestStreamFlags(SSF_CONNECTING))
    return EALREADY;

  if (IsConnected())
    return EISCONN;

  remote_addr_ = SockAddrToResource(addr, len);
  if (0 == remote_addr_)
    return EINVAL;

  int ms = attr.IsBlocking() ? -1 : 0;

  SetStreamFlags(SSF_CONNECTING);
  QueueConnect();

  Error err = wait.WaitOnEvent(POLLOUT, ms);
  if (ETIMEDOUT == err)
    return EINPROGRESS;

  // If we fail, release the dest addr resource
  if (err != 0) {
    ConnectFailed_Locked();
    return err;
  }

  // Make sure the connection succeeded.
  if (last_errno_ != 0) {
    ConnectFailed_Locked();
    return last_errno_;
  }

  ConnectDone_Locked();
  return 0;
}

Error TcpNode::Shutdown(int how) {
  AUTO_LOCK(node_lock_);
  if (!IsConnected())
    return ENOTCONN;

  {
    AUTO_LOCK(emitter_->GetLock());
    emitter_->SetError_Locked();
  }
  return 0;
}

void TcpNode::ConnectDone_Locked() {
  local_addr_ = TCPInterface()->GetLocalAddress(socket_resource_);

  // Now that we are connected, we can start sending and receiving.
  ClearStreamFlags(SSF_CONNECTING | SSF_CAN_CONNECT);
  SetStreamFlags(SSF_CAN_SEND | SSF_CAN_RECV);

  emitter_->ConnectDone_Locked();

  // The NODELAY option cannot be set in PPAPI before the socket
  // is connected, but setsockopt() might have already set it.
  SetNoDelay_Locked();

  // Begin the input pump
  QueueInput();
}

void TcpNode::ConnectFailed_Locked() {
  filesystem_->ppapi()->ReleaseResource(remote_addr_);
  remote_addr_ = 0;
}

Error TcpNode::Listen(int backlog) {
  AUTO_LOCK(node_lock_);
  if (!IsBound())
    return EINVAL;

  int err = TCPInterface()->Listen(
      socket_resource_, backlog, PP_BlockUntilComplete());
  if (err != PP_OK)
    return PPERROR_TO_ERRNO(err);

  ClearStreamFlags(SSF_CAN_CONNECT);
  SetStreamFlags(SSF_LISTENING);
  emitter_->SetListening_Locked();
  QueueAccept();
  return 0;
}

Error TcpNode::Recv_Locked(void* buf,
                           size_t len,
                           PP_Resource* out_addr,
                           int* out_len) {
  assert(emitter_.get());
  *out_len = emitter_->ReadIn_Locked((char*)buf, len);
  *out_addr = remote_addr_;

  // Ref the address copy we pass back.
  filesystem_->ppapi()->AddRefResource(remote_addr_);
  return 0;
}

// TCP ignores dst addr passed to send_to, and always uses bound address
Error TcpNode::Send_Locked(const void* buf,
                           size_t len,
                           PP_Resource,
                           int* out_len) {
  assert(emitter_.get());
  if (emitter_->GetError_Locked())
    return EPIPE;
  *out_len = emitter_->WriteOut_Locked((char*)buf, len);
  return 0;
}

}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API
