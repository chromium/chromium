// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKET_SOCKET_API_H_
#define EXTENSIONS_BROWSER_API_SOCKET_SOCKET_API_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_set>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/async_api_function.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/socket.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/network_change_notifier.h"
#include "net/socket/tcp_client_socket.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"

#if defined(OS_CHROMEOS)
#include "extensions/browser/api/socket/app_firewall_hole_manager.h"
#endif  // OS_CHROMEOS

namespace content {
class BrowserContext;
}

namespace net {
class IOBuffer;
}

namespace extensions {
class Socket;

// A simple interface to ApiResourceManager<Socket> or derived class. The goal
// of this interface is to allow Socket API functions to use distinct instances
// of ApiResourceManager<> depending on the type of socket (old version in
// "socket" namespace vs new version in "socket.xxx" namespaces).
class SocketResourceManagerInterface {
 public:
  virtual ~SocketResourceManagerInterface() {}

  virtual bool SetBrowserContext(content::BrowserContext* context) = 0;
  virtual int Add(Socket* socket) = 0;
  virtual Socket* Get(const std::string& extension_id, int api_resource_id) = 0;
  virtual void Remove(const std::string& extension_id, int api_resource_id) = 0;
  virtual void Replace(const std::string& extension_id,
                       int api_resource_id,
                       Socket* socket) = 0;
  virtual std::unordered_set<int>* GetResourceIds(
      const std::string& extension_id) = 0;
};

// Implementation of SocketResourceManagerInterface using an
// ApiResourceManager<T> instance (where T derives from Socket).
template <typename T>
class SocketResourceManager : public SocketResourceManagerInterface {
 public:
  SocketResourceManager() : manager_(NULL) {}

  bool SetBrowserContext(content::BrowserContext* context) override {
    manager_ = ApiResourceManager<T>::Get(context);
    DCHECK(manager_)
        << "There is no socket manager. "
           "If this assertion is failing during a test, then it is likely that "
           "TestExtensionSystem is failing to provide an instance of "
           "ApiResourceManager<Socket>.";
    return manager_ != NULL;
  }

  int Add(Socket* socket) override {
    // Note: Cast needed here, because "T" may be a subclass of "Socket".
    return manager_->Add(static_cast<T*>(socket));
  }

  Socket* Get(const std::string& extension_id, int api_resource_id) override {
    return manager_->Get(extension_id, api_resource_id);
  }

  void Replace(const std::string& extension_id,
               int api_resource_id,
               Socket* socket) override {
    manager_->Replace(extension_id, api_resource_id, static_cast<T*>(socket));
  }

  void Remove(const std::string& extension_id, int api_resource_id) override {
    manager_->Remove(extension_id, api_resource_id);
  }

  std::unordered_set<int>* GetResourceIds(
      const std::string& extension_id) override {
    return manager_->GetResourceIds(extension_id);
  }

 private:
  ApiResourceManager<T>* manager_;
};

class SocketAsyncApiFunction : public AsyncApiFunction {
 public:
  SocketAsyncApiFunction();

 protected:
  ~SocketAsyncApiFunction() override;

  // AsyncApiFunction:
  bool PrePrepare() override;
  bool Respond() override;

  virtual std::unique_ptr<SocketResourceManagerInterface>
  CreateSocketResourceManager();

  int AddSocket(Socket* socket);
  Socket* GetSocket(int api_resource_id);
  void ReplaceSocket(int api_resource_id, Socket* socket);
  void RemoveSocket(int api_resource_id);
  std::unordered_set<int>* GetSocketIds();

  // A no-op outside of Chrome OS.
  void OpenFirewallHole(const std::string& address,
                        int socket_id,
                        Socket* socket);

 private:
#if defined(OS_CHROMEOS)
  void OpenFirewallHoleOnUIThread(AppFirewallHole::PortType type,
                                  uint16_t port,
                                  int socket_id);
  void OnFirewallHoleOpened(
      int socket_id,
      std::unique_ptr<AppFirewallHole, content::BrowserThread::DeleteOnUIThread>
          hole);
#endif  // OS_CHROMEOS

  std::unique_ptr<SocketResourceManagerInterface> manager_;
};

class SocketExtensionWithDnsLookupFunction
    : public SocketAsyncApiFunction,
      public network::ResolveHostClientBase {
 protected:
  SocketExtensionWithDnsLookupFunction();
  ~SocketExtensionWithDnsLookupFunction() override;

  // AsyncApiFunction:
  bool PrePrepare() override;

  void StartDnsLookup(const net::HostPortPair& host_port_pair);
  virtual void AfterDnsLookup(int lookup_result) = 0;

  net::AddressList addresses_;

 private:
  // network::mojom::ResolveHostClient implementation:
  void OnComplete(
      int result,
      const base::Optional<net::AddressList>& resolved_addresses) override;

  mojo::PendingRemote<network::mojom::HostResolver> pending_host_resolver_;
  mojo::Remote<network::mojom::HostResolver> host_resolver_;

  // A reference to |this| must be taken while the request is being made on this
  // receiver so the object is alive when the request completes.
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
};

class SocketCreateFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.create", SOCKET_CREATE)

  SocketCreateFunction();

 protected:
  ~SocketCreateFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SocketUnitTest, Create);
  enum SocketType { kSocketTypeInvalid = -1, kSocketTypeTCP, kSocketTypeUDP };

  // These two fields are only applicable if |socket_type_| is UDP.
  mojo::PendingRemote<network::mojom::UDPSocket> socket_;
  mojo::PendingReceiver<network::mojom::UDPSocketListener>
      socket_listener_receiver_;

  std::unique_ptr<api::socket::Create::Params> params_;
  SocketType socket_type_;
};

class SocketDestroyFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.destroy", SOCKET_DESTROY)

 protected:
  ~SocketDestroyFunction() override {}

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  int socket_id_;
};

class SocketConnectFunction : public SocketExtensionWithDnsLookupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.connect", SOCKET_CONNECT)

  SocketConnectFunction();

 protected:
  ~SocketConnectFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

  // SocketExtensionWithDnsLookupFunction:
  void AfterDnsLookup(int lookup_result) override;

 private:
  void StartConnect();
  void OnConnect(int result);

  int socket_id_;
  std::string hostname_;
  uint16_t port_;
};

class SocketDisconnectFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.disconnect", SOCKET_DISCONNECT)

 protected:
  ~SocketDisconnectFunction() override {}

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  int socket_id_;
};

class SocketBindFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.bind", SOCKET_BIND)

 protected:
  ~SocketBindFunction() override {}

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void OnCompleted(int net_error);

  int socket_id_;
  std::string address_;
  uint16_t port_;
};

class SocketListenFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.listen", SOCKET_LISTEN)

  SocketListenFunction();

 protected:
  ~SocketListenFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void OnCompleted(int result, const std::string& error_msg);
  std::unique_ptr<api::socket::Listen::Params> params_;
};

class SocketAcceptFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.accept", SOCKET_ACCEPT)

  SocketAcceptFunction();

 protected:
  ~SocketAcceptFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void OnAccept(int result_code,
                mojo::PendingRemote<network::mojom::TCPConnectedSocket> socket,
                const base::Optional<net::IPEndPoint>& remote_addr,
                mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
                mojo::ScopedDataPipeProducerHandle send_pipe_handle);

  std::unique_ptr<api::socket::Accept::Params> params_;
};

class SocketReadFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.read", SOCKET_READ)

  SocketReadFunction();

 protected:
  ~SocketReadFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;
  void OnCompleted(int result,
                   scoped_refptr<net::IOBuffer> io_buffer,
                   bool socket_destroying);

 private:
  std::unique_ptr<api::socket::Read::Params> params_;
};

class SocketWriteFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.write", SOCKET_WRITE)

  SocketWriteFunction();

 protected:
  ~SocketWriteFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;
  void OnCompleted(int result);

 private:
  int socket_id_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_size_;
};

class SocketRecvFromFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.recvFrom", SOCKET_RECVFROM)

  SocketRecvFromFunction();

 protected:
  ~SocketRecvFromFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void AsyncWorkStart() override;
  void OnCompleted(int result,
                   scoped_refptr<net::IOBuffer> io_buffer,
                   bool socket_destroying,
                   const std::string& address,
                   uint16_t port);

 private:
  std::unique_ptr<api::socket::RecvFrom::Params> params_;
};

class SocketSendToFunction : public SocketExtensionWithDnsLookupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.sendTo", SOCKET_SENDTO)

  SocketSendToFunction();

 protected:
  ~SocketSendToFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;
  void OnCompleted(int result);

  // SocketExtensionWithDnsLookupFunction:
  void AfterDnsLookup(int lookup_result) override;

 private:
  void StartSendTo();

  int socket_id_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_size_;
  std::string hostname_;
  uint16_t port_;
};

class SocketSetKeepAliveFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.setKeepAlive", SOCKET_SETKEEPALIVE)

  SocketSetKeepAliveFunction();

 protected:
  ~SocketSetKeepAliveFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void OnCompleted(bool success);

  std::unique_ptr<api::socket::SetKeepAlive::Params> params_;
};

class SocketSetNoDelayFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.setNoDelay", SOCKET_SETNODELAY)

  SocketSetNoDelayFunction();

 protected:
  ~SocketSetNoDelayFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void OnCompleted(bool success);

  std::unique_ptr<api::socket::SetNoDelay::Params> params_;
};

class SocketGetInfoFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.getInfo", SOCKET_GETINFO)

  SocketGetInfoFunction();

 protected:
  ~SocketGetInfoFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<api::socket::GetInfo::Params> params_;
};

class SocketGetNetworkListFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.getNetworkList", SOCKET_GETNETWORKLIST)

 protected:
  ~SocketGetNetworkListFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void GotNetworkList(
      const base::Optional<net::NetworkInterfaceList>& interface_list);
};

class SocketJoinGroupFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.joinGroup", SOCKET_MULTICAST_JOIN_GROUP)

  SocketJoinGroupFunction();

 protected:
  ~SocketJoinGroupFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void OnCompleted(int result);

  std::unique_ptr<api::socket::JoinGroup::Params> params_;
};

class SocketLeaveGroupFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.leaveGroup", SOCKET_MULTICAST_LEAVE_GROUP)

  SocketLeaveGroupFunction();

 protected:
  ~SocketLeaveGroupFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void OnCompleted(int result);

  std::unique_ptr<api::socket::LeaveGroup::Params> params_;
};

class SocketSetMulticastTimeToLiveFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.setMulticastTimeToLive",
                             SOCKET_MULTICAST_SET_TIME_TO_LIVE)

  SocketSetMulticastTimeToLiveFunction();

 protected:
  ~SocketSetMulticastTimeToLiveFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<api::socket::SetMulticastTimeToLive::Params> params_;
};

class SocketSetMulticastLoopbackModeFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.setMulticastLoopbackMode",
                             SOCKET_MULTICAST_SET_LOOPBACK_MODE)

  SocketSetMulticastLoopbackModeFunction();

 protected:
  ~SocketSetMulticastLoopbackModeFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<api::socket::SetMulticastLoopbackMode::Params> params_;
};

class SocketGetJoinedGroupsFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.getJoinedGroups",
                             SOCKET_MULTICAST_GET_JOINED_GROUPS)

  SocketGetJoinedGroupsFunction();

 protected:
  ~SocketGetJoinedGroupsFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<api::socket::GetJoinedGroups::Params> params_;
};

class SocketSecureFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.secure", SOCKET_SECURE)
  SocketSecureFunction();

 protected:
  ~SocketSecureFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void TlsConnectDone(
      int result,
      mojo::PendingRemote<network::mojom::TLSClientSocket> tls_socket,
      const net::IPEndPoint& local_addr,
      const net::IPEndPoint& peer_addr,
      mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
      mojo::ScopedDataPipeProducerHandle send_pipe_handle);

  std::unique_ptr<api::socket::Secure::Params> params_;

  DISALLOW_COPY_AND_ASSIGN(SocketSecureFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKET_SOCKET_API_H_
