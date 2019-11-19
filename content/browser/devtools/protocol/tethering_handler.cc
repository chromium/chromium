// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/tethering_handler.h"

#include <map>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/server_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace content {
namespace protocol {

using BindCallback = Tethering::Backend::BindCallback;
using UnbindCallback = Tethering::Backend::UnbindCallback;

namespace {

const int kListenBacklog = 5;
const int kSocketPumpBufferSize = 16 * 1024;

const int kMinTetheringPort = 1024;
const int kMaxTetheringPort = 65535;

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("tethering_handler_socket", R"(
        semantics {
          sender: "Tethering Handler"
          description:
            "For remote debugging local Android device, one might need to "
            "enable reverse tethering for forwarding local ports from the "
            "device to some ports on the host. This socket pumps the traffic "
            "between the two."
          trigger:
            "A user connects to an Android device using remote debugging and "
            "enables port forwarding on chrome://inspect."
          data: "Any data requested from the local port on Android device."
          destination: OTHER
          destination_other:
            "Data is sent to the target that user selects in chrome://inspect."
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This request cannot be disabled in settings, however it would be "
            "sent only if user enables port fowarding in chrome://inspect and "
            "USB debugging in the Android device system settings."
          policy_exception_justification:
            "Not implemented, policies defined on Android device will apply "
            "here."
        })");

using CreateServerSocketCallback =
    base::Callback<std::unique_ptr<net::ServerSocket>(std::string*)>;

class SocketPump {
 public:
  SocketPump(net::StreamSocket* client_socket)
      : client_socket_(client_socket),
        pending_writes_(0),
        pending_destruction_(false) {
  }

  std::string Init(const CreateServerSocketCallback& socket_callback) {
    std::string channel_name;
    server_socket_ = socket_callback.Run(&channel_name);
    if (!server_socket_.get() || channel_name.empty()) {
      SelfDestruct();
      return std::string();
    }

    int result = server_socket_->Accept(
        &accepted_socket_,
        base::BindOnce(&SocketPump::OnAccepted, base::Unretained(this)));
    if (result != net::ERR_IO_PENDING)
      OnAccepted(result);
    return channel_name;
  }

 private:
  void OnAccepted(int result) {
    if (result < 0) {
      SelfDestruct();
      return;
    }

    ++pending_writes_; // avoid SelfDestruct in first Pump
    Pump(client_socket_.get(), accepted_socket_.get());
    --pending_writes_;
    if (pending_destruction_) {
      SelfDestruct();
    } else {
      Pump(accepted_socket_.get(), client_socket_.get());
    }
  }

  void Pump(net::StreamSocket* from, net::StreamSocket* to) {
    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(kSocketPumpBufferSize);
    int result =
        from->Read(buffer.get(), kSocketPumpBufferSize,
                   base::BindOnce(&SocketPump::OnRead, base::Unretained(this),
                                  from, to, buffer));
    if (result != net::ERR_IO_PENDING)
      OnRead(from, to, buffer, result);
  }

  void OnRead(net::StreamSocket* from,
              net::StreamSocket* to,
              scoped_refptr<net::IOBuffer> buffer,
              int result) {
    if (result <= 0) {
      SelfDestruct();
      return;
    }

    int total = result;
    scoped_refptr<net::DrainableIOBuffer> drainable =
        base::MakeRefCounted<net::DrainableIOBuffer>(std::move(buffer), total);

    ++pending_writes_;
    result =
        to->Write(drainable.get(), total,
                  base::BindOnce(&SocketPump::OnWritten, base::Unretained(this),
                                 drainable, from, to),
                  kTrafficAnnotation);
    if (result != net::ERR_IO_PENDING)
      OnWritten(drainable, from, to, result);
  }

  void OnWritten(scoped_refptr<net::DrainableIOBuffer> drainable,
                 net::StreamSocket* from,
                 net::StreamSocket* to,
                 int result) {
    --pending_writes_;
    if (result < 0) {
      SelfDestruct();
      return;
    }

    drainable->DidConsume(result);
    if (drainable->BytesRemaining() > 0) {
      ++pending_writes_;
      result =
          to->Write(drainable.get(), drainable->BytesRemaining(),
                    base::BindOnce(&SocketPump::OnWritten,
                                   base::Unretained(this), drainable, from, to),
                    kTrafficAnnotation);
      if (result != net::ERR_IO_PENDING)
        OnWritten(drainable, from, to, result);
      return;
    }

    if (pending_destruction_) {
      SelfDestruct();
      return;
    }
    Pump(from, to);
  }

  void SelfDestruct() {
    if (pending_writes_ > 0) {
      pending_destruction_ = true;
      return;
    }
    delete this;
  }


 private:
  std::unique_ptr<net::StreamSocket> client_socket_;
  std::unique_ptr<net::ServerSocket> server_socket_;
  std::unique_ptr<net::StreamSocket> accepted_socket_;
  int pending_writes_;
  bool pending_destruction_;
};

class BoundSocket {
 public:
  typedef base::Callback<void(uint16_t, const std::string&)> AcceptedCallback;

  BoundSocket(AcceptedCallback accepted_callback,
              const CreateServerSocketCallback& socket_callback)
      : accepted_callback_(accepted_callback),
        socket_callback_(socket_callback),
        socket_(new net::TCPServerSocket(nullptr, net::NetLogSource())),
        port_(0) {}

  virtual ~BoundSocket() {
  }

  bool Listen(uint16_t port) {
    port_ = port;
    net::IPEndPoint end_point(net::IPAddress::IPv4Localhost(), port);
    int result = socket_->Listen(end_point, kListenBacklog);
    if (result < 0)
      return false;

    net::IPEndPoint local_address;
    result = socket_->GetLocalAddress(&local_address);
    if (result < 0)
      return false;

    DoAccept();
    return true;
  }

 private:
  typedef std::map<net::IPEndPoint, net::StreamSocket*> AcceptedSocketsMap;

  void DoAccept() {
    while (true) {
      int result = socket_->Accept(
          &accept_socket_,
          base::BindOnce(&BoundSocket::OnAccepted, base::Unretained(this)));
      if (result == net::ERR_IO_PENDING)
        break;
      else
        HandleAcceptResult(result);
    }
  }

  void OnAccepted(int result) {
    HandleAcceptResult(result);
    if (result == net::OK)
      DoAccept();
  }

  void HandleAcceptResult(int result) {
    if (result != net::OK)
      return;

    SocketPump* pump = new SocketPump(accept_socket_.release());
    std::string name = pump->Init(socket_callback_);
    if (!name.empty())
      accepted_callback_.Run(port_, name);
  }

  AcceptedCallback accepted_callback_;
  CreateServerSocketCallback socket_callback_;
  std::unique_ptr<net::ServerSocket> socket_;
  std::unique_ptr<net::StreamSocket> accept_socket_;
  uint16_t port_;
};

}  // namespace

// TetheringHandler::TetheringImpl -------------------------------------------

class TetheringHandler::TetheringImpl {
 public:
  TetheringImpl(
      base::WeakPtr<TetheringHandler> handler,
      const CreateServerSocketCallback& socket_callback);
  ~TetheringImpl();

  void Bind(uint16_t port, std::unique_ptr<BindCallback> callback);
  void Unbind(uint16_t port, std::unique_ptr<UnbindCallback> callback);
  void Accepted(uint16_t port, const std::string& name);

 private:
  base::WeakPtr<TetheringHandler> handler_;
  CreateServerSocketCallback socket_callback_;
  std::map<uint16_t, std::unique_ptr<BoundSocket>> bound_sockets_;
};

TetheringHandler::TetheringImpl::TetheringImpl(
    base::WeakPtr<TetheringHandler> handler,
    const CreateServerSocketCallback& socket_callback)
    : handler_(handler),
      socket_callback_(socket_callback) {
}

TetheringHandler::TetheringImpl::~TetheringImpl() = default;

void TetheringHandler::TetheringImpl::Bind(
    uint16_t port, std::unique_ptr<BindCallback> callback) {
  if (bound_sockets_.find(port) != bound_sockets_.end()) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&BindCallback::sendFailure, std::move(callback),
                       Response::Error("Port already bound")));
    return;
  }

  BoundSocket::AcceptedCallback accepted = base::Bind(
      &TetheringHandler::TetheringImpl::Accepted, base::Unretained(this));
  std::unique_ptr<BoundSocket> bound_socket =
      std::make_unique<BoundSocket>(std::move(accepted), socket_callback_);
  if (!bound_socket->Listen(port)) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&BindCallback::sendFailure, std::move(callback),
                       Response::Error("Could not bind port")));
    return;
  }

  bound_sockets_[port] = std::move(bound_socket);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&BindCallback::sendSuccess, std::move(callback)));
}

void TetheringHandler::TetheringImpl::Unbind(
    uint16_t port, std::unique_ptr<UnbindCallback> callback) {
  auto it = bound_sockets_.find(port);
  if (it == bound_sockets_.end()) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&UnbindCallback::sendFailure, std::move(callback),
                       Response::InvalidParams("Port is not bound")));
    return;
  }

  bound_sockets_.erase(it);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&UnbindCallback::sendSuccess, std::move(callback)));
}

void TetheringHandler::TetheringImpl::Accepted(uint16_t port,
                                               const std::string& name) {
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&TetheringHandler::Accepted, handler_, port, name));
}


// TetheringHandler ----------------------------------------------------------

// static
TetheringHandler::TetheringImpl* TetheringHandler::impl_ = nullptr;

TetheringHandler::TetheringHandler(
    const CreateServerSocketCallback& socket_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : DevToolsDomainHandler(Tethering::Metainfo::domainName),
      socket_callback_(socket_callback),
      task_runner_(task_runner),
      is_active_(false) {}

TetheringHandler::~TetheringHandler() {
  if (is_active_) {
    task_runner_->DeleteSoon(FROM_HERE, impl_);
    impl_ = nullptr;
  }
}

void TetheringHandler::Wire(UberDispatcher* dispatcher) {
  frontend_.reset(new Tethering::Frontend(dispatcher->channel()));
  Tethering::Dispatcher::wire(dispatcher, this);
}

void TetheringHandler::Accepted(uint16_t port, const std::string& name) {
  frontend_->Accepted(port, name);
}

bool TetheringHandler::Activate() {
  if (is_active_)
    return true;
  if (impl_)
    return false;
  is_active_ = true;
  impl_ = new TetheringImpl(weak_factory_.GetWeakPtr(), socket_callback_);
  return true;
}

void TetheringHandler::Bind(
    int port, std::unique_ptr<BindCallback> callback) {
  if (port < kMinTetheringPort || port > kMaxTetheringPort) {
    callback->sendFailure(Response::InvalidParams("port"));
    return;
  }

  if (!Activate()) {
    callback->sendFailure(
        Response::Error("Tethering is used by another connection"));
    return;
  }

  DCHECK(impl_);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TetheringImpl::Bind, base::Unretained(impl_),
                                port, std::move(callback)));
}

void TetheringHandler::Unbind(
    int port, std::unique_ptr<UnbindCallback> callback) {
  if (!Activate()) {
    callback->sendFailure(
        Response::Error("Tethering is used by another connection"));
    return;
  }

  DCHECK(impl_);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TetheringImpl::Unbind, base::Unretained(impl_),
                                port, std::move(callback)));
}

}  // namespace protocol
}  // namespace content
