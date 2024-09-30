// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKET_SOCKET_API_H_
#define EXTENSIONS_BROWSER_API_SOCKET_SOCKET_API_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/socket_permission_request.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/socket.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/api_permission.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/socket/tcp_client_socket.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace net {
class IOBuffer;
}  // namespace net

namespace network::mojom {
class TLSClientSocket;
class TCPConnectedSocket;
}  // namespace network::mojom

namespace extensions {

#if BUILDFLAG(IS_CHROMEOS)
extern const char kCrOSTerminal[];
#endif  // BUILDFLAG(IS_CHROMEOS)

class Socket;

// A simple interface to ApiResourceManager<Socket> or derived class. The goal
// of this interface is to allow Socket API functions to use distinct instances
// of ApiResourceManager<> depending on the type of socket (old version in
// "socket" namespace vs new version in "socket.xxx" namespaces).
class SocketResourceManagerInterface {
 public:
  virtual ~SocketResourceManagerInterface() = default;

  virtual bool SetBrowserContext(content::BrowserContext* context) = 0;
  virtual int Add(Socket* socket) = 0;
  virtual Socket* Get(const ExtensionId& extension_id, int api_resource_id) = 0;
  virtual void Remove(const ExtensionId& extension_id, int api_resource_id) = 0;
  virtual void Replace(const ExtensionId& extension_id,
                       int api_resource_id,
                       Socket* socket) = 0;
  virtual std::unordered_set<int>* GetResourceIds(
      const ExtensionId& extension_id) = 0;
};

// Implementation of SocketResourceManagerInterface using an
// ApiResourceManager<T> instance (where T derives from Socket).
template <typename T>
class SocketResourceManager : public SocketResourceManagerInterface {
 public:
  SocketResourceManager() : manager_(nullptr) {}

  bool SetBrowserContext(content::BrowserContext* context) override {
    manager_ = ApiResourceManager<T>::Get(context);
    DCHECK(manager_)
        << "There is no socket manager. "
           "If this assertion is failing during a test, then it is likely that "
           "TestExtensionSystem is failing to provide an instance of "
           "ApiResourceManager<Socket>.";
    return !!manager_;
  }

  int Add(Socket* socket) override {
    // Note: Cast needed here, because "T" may be a subclass of "Socket".
    return manager_->Add(static_cast<T*>(socket));
  }

  Socket* Get(const ExtensionId& extension_id, int api_resource_id) override {
    return manager_->Get(extension_id, api_resource_id);
  }

  void Replace(const ExtensionId& extension_id,
               int api_resource_id,
               Socket* socket) override {
    manager_->Replace(extension_id, api_resource_id, static_cast<T*>(socket));
  }

  void Remove(const ExtensionId& extension_id, int api_resource_id) override {
    manager_->Remove(extension_id, api_resource_id);
  }

  std::unordered_set<int>* GetResourceIds(
      const ExtensionId& extension_id) override {
    return manager_->GetResourceIds(extension_id);
  }

 private:
  raw_ptr<ApiResourceManager<T>> manager_;
};

// Base class for socket API functions, with some helper functions.
class SocketApiFunction : public ExtensionFunction {
 public:
  inline static constexpr char kExceedWriteQuotaError[] =
      "Exceeded write quota.";

  SocketApiFunction();

 protected:
  ~SocketApiFunction() override;

  // Subclasses should implement this instead of Run().
  virtual ResponseAction Work() = 0;

  // ExtensionFunction:
  ResponseAction Run() final;

  // Convenience wrapper for ErrorWithArguments(), where the arguments are just
  // one integer value.
  ResponseValue ErrorWithCode(int error_code, const std::string& error);

  // Either extension_id() or url origin for CrOS Terminal.
  std::string GetOriginId() const;

  // Checks extension()->permissions_data(), or returns true for CrOS Terminal.
  bool CheckPermission(const APIPermission::CheckParam& param) const;

  // Checks SocketsManifestData::CheckRequest() if extension(), or returns true
  // for CrOS Terminal.
  bool CheckRequest(const content::SocketPermissionRequest& param) const;

  // Adds `bytes_to_write` against the write quota. Returns false if it would
  // exceed the write quota.
  bool TakeWriteQuota(size_t bytes_to_write);

  // Returns bytes taken in last `TakeWriteQuota` call to the write quota.
  void ReturnWriteQuota();

  virtual std::unique_ptr<SocketResourceManagerInterface>
  CreateSocketResourceManager();

  int AddSocket(Socket* socket);
  Socket* GetSocket(int api_resource_id);
  void ReplaceSocket(int api_resource_id, Socket* socket);
  void RemoveSocket(int api_resource_id);
  std::unordered_set<int>* GetSocketIds();

  // A no-op outside of Chrome OS. Calls Respond() with an error if it fails.
  void OpenFirewallHole(const std::string& address,
                        int socket_id,
                        Socket* socket);

 private:
  class ScopedWriteQuota {
   public:
    ScopedWriteQuota(SocketApiFunction* owner, size_t bytes_used);
    ~ScopedWriteQuota();

   private:
    const raw_ptr<SocketApiFunction> owner_;
    const size_t bytes_used_;
  };

  std::unique_ptr<SocketResourceManagerInterface> manager_;
  std::optional<ScopedWriteQuota> write_quota_used_;
};

class SocketExtensionWithDnsLookupFunction
    : public SocketApiFunction,
      public network::ResolveHostClientBase {
 protected:
  SocketExtensionWithDnsLookupFunction();
  ~SocketExtensionWithDnsLookupFunction() override;

  void StartDnsLookup(const net::HostPortPair& host_port_pair,
                      net::DnsQueryType dns_query_type);
  virtual void AfterDnsLookup(int lookup_result) = 0;

  net::AddressList addresses_;

 private:
  // network::mojom::ResolveHostClient implementation:
  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const std::optional<net::AddressList>& resolved_addresses,
                  const std::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override;

  mojo::PendingRemote<network::mojom::HostResolver> pending_host_resolver_;
  mojo::Remote<network::mojom::HostResolver> host_resolver_;

  // A reference to |this| must be taken while the request is being made on this
  // receiver so the object is alive when the request completes.
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
};

class SocketCreateFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.create", SOCKET_CREATE)

  SocketCreateFunction();

 protected:
  ~SocketCreateFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SocketUnitTest, Create);
};

class SocketDestroyFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.destroy", SOCKET_DESTROY)

 protected:
  ~SocketDestroyFunction() override {}

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketConnectFunction : public SocketExtensionWithDnsLookupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.connect", SOCKET_CONNECT)

  SocketConnectFunction();

 protected:
  ~SocketConnectFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;

  // SocketExtensionWithDnsLookupFunction:
  void AfterDnsLookup(int lookup_result) override;

 private:
  void StartConnect();
  void OnConnect(int result);

  int socket_id_ = 0;
  std::string hostname_;
  uint16_t port_ = 0;
};

class SocketDisconnectFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.disconnect", SOCKET_DISCONNECT)

 protected:
  ~SocketDisconnectFunction() override {}

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketBindFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.bind", SOCKET_BIND)

 protected:
  ~SocketBindFunction() override {}

  // SocketApiFunction:
  ResponseAction Work() override;

 private:
  void OnCompleted(int net_error);

  int socket_id_;
  std::string address_;
  uint16_t port_;
};

class SocketListenFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.listen", SOCKET_LISTEN)

  SocketListenFunction();

 protected:
  ~SocketListenFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;

 private:
  void OnCompleted(int result, const std::string& error_msg);
  std::optional<api::socket::Listen::Params> params_;
};

class SocketAcceptFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.accept", SOCKET_ACCEPT)

  SocketAcceptFunction();

 protected:
  ~SocketAcceptFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;

 private:
  void OnAccept(int result_code,
                mojo::PendingRemote<network::mojom::TCPConnectedSocket> socket,
                const std::optional<net::IPEndPoint>& remote_addr,
                mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
                mojo::ScopedDataPipeProducerHandle send_pipe_handle);
};

class SocketReadFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.read", SOCKET_READ)

  SocketReadFunction();

 protected:
  ~SocketReadFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
  void OnCompleted(int result,
                   scoped_refptr<net::IOBuffer> io_buffer,
                   bool socket_destroying);
};

class SocketWriteFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.write", SOCKET_WRITE)

  SocketWriteFunction();

 protected:
  ~SocketWriteFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
  void OnCompleted(int result);
};

class SocketRecvFromFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.recvFrom", SOCKET_RECVFROM)

  SocketRecvFromFunction();

 protected:
  ~SocketRecvFromFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
  void OnCompleted(int result,
                   scoped_refptr<net::IOBuffer> io_buffer,
                   bool socket_destroying,
                   const std::string& address,
                   uint16_t port);
};

class SocketSendToFunction : public SocketExtensionWithDnsLookupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.sendTo", SOCKET_SENDTO)

  SocketSendToFunction();

 protected:
  ~SocketSendToFunction() override;

  // SocketApiFunction::
  ResponseAction Work() override;
  void OnCompleted(int result);

  // SocketExtensionWithDnsLookupFunction:
  void AfterDnsLookup(int lookup_result) override;

 private:
  void StartSendTo();

  int socket_id_ = 0;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_size_ = 0;
  std::string hostname_;
  uint16_t port_ = 0;
};

class SocketSetKeepAliveFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.setKeepAlive", SOCKET_SETKEEPALIVE)

  SocketSetKeepAliveFunction();

 protected:
  ~SocketSetKeepAliveFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;

 private:
  void OnCompleted(bool success);
};

class SocketSetNoDelayFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.setNoDelay", SOCKET_SETNODELAY)

  SocketSetNoDelayFunction();

 protected:
  ~SocketSetNoDelayFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;

 private:
  void OnCompleted(bool success);

  std::optional<api::socket::SetNoDelay::Params> params_;
};

class SocketGetInfoFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.getInfo", SOCKET_GETINFO)

  SocketGetInfoFunction();

 protected:
  ~SocketGetInfoFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
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
      const std::optional<net::NetworkInterfaceList>& interface_list);
};

class SocketJoinGroupFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.joinGroup", SOCKET_MULTICAST_JOIN_GROUP)

  SocketJoinGroupFunction();

 protected:
  ~SocketJoinGroupFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;

 private:
  void OnCompleted(int result);
};

class SocketLeaveGroupFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.leaveGroup", SOCKET_MULTICAST_LEAVE_GROUP)

  SocketLeaveGroupFunction();

 protected:
  ~SocketLeaveGroupFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;

 private:
  void OnCompleted(int result);
};

class SocketSetMulticastTimeToLiveFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.setMulticastTimeToLive",
                             SOCKET_MULTICAST_SET_TIME_TO_LIVE)

  SocketSetMulticastTimeToLiveFunction();

 protected:
  ~SocketSetMulticastTimeToLiveFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketSetMulticastLoopbackModeFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.setMulticastLoopbackMode",
                             SOCKET_MULTICAST_SET_LOOPBACK_MODE)

  SocketSetMulticastLoopbackModeFunction();

 protected:
  ~SocketSetMulticastLoopbackModeFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketGetJoinedGroupsFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.getJoinedGroups",
                             SOCKET_MULTICAST_GET_JOINED_GROUPS)

  SocketGetJoinedGroupsFunction();

 protected:
  ~SocketGetJoinedGroupsFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketSecureFunction : public SocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.secure", SOCKET_SECURE)
  SocketSecureFunction();

  SocketSecureFunction(const SocketSecureFunction&) = delete;
  SocketSecureFunction& operator=(const SocketSecureFunction&) = delete;

 protected:
  ~SocketSecureFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;

 private:
  void TlsConnectDone(
      int result,
      mojo::PendingRemote<network::mojom::TLSClientSocket> tls_socket,
      const net::IPEndPoint& local_addr,
      const net::IPEndPoint& peer_addr,
      mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
      mojo::ScopedDataPipeProducerHandle send_pipe_handle);

  std::optional<api::socket::Secure::Params> params_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKET_SOCKET_API_H_
