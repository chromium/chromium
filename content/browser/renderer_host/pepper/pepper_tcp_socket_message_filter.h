// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SOCKET_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SOCKET_MESSAGE_FILTER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/public/host_resolver_results.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/host/resource_message_filter.h"
#include "ppapi/shared_impl/ppb_tcp_socket_shared.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"

namespace ppapi {
class SocketOptionData;

namespace host {
struct ReplyMessageContext;
}
}  // namespace ppapi

namespace chromeos {
class FirewallHole;
}  // namespace chromeos

namespace content {

class BrowserPpapiHostImpl;
class ContentBrowserPepperHostFactory;

// Handles communication between Pepper and TCP socket Mojo interfaces. The Mojo
// interfaces and all class variables live on the UI thread, while the class is
// created on and receives IPCs on the IO thread (The IPCs are then passed to
// the UI thread).
class CONTENT_EXPORT PepperTCPSocketMessageFilter
    : public ppapi::host::ResourceMessageFilter,
      public BrowserPpapiHostImpl::InstanceObserver,
      public network::ResolveHostClientBase,
      public network::mojom::SocketObserver {
 public:
  // |factory| must be non-nullptr unless the consumer immediately calls
  // SetConnectedSocket(). SetConnectedSocket() must be a separate method,
  // because something must already be holding onto a reference to |this| when a
  // task is posted to the UI thread (Which requires grabbing another reference,
  // which could potentially be released before the constructor returns).
  PepperTCPSocketMessageFilter(ContentBrowserPepperHostFactory* factory,
                               BrowserPpapiHostImpl* host,
                               PP_Instance instance,
                               ppapi::TCPSocketVersion version);

  PepperTCPSocketMessageFilter(const PepperTCPSocketMessageFilter&) = delete;
  PepperTCPSocketMessageFilter& operator=(const PepperTCPSocketMessageFilter&) =
      delete;

  // Switches state to CONNECTED using the provided pipes. May only be called
  // before any messages are received,
  void SetConnectedSocket(
      mojo::PendingRemote<network::mojom::TCPConnectedSocket> connected_socket,
      mojo::PendingReceiver<network::mojom::SocketObserver>
          socket_observer_receiver,
      mojo::ScopedDataPipeConsumerHandle receive_stream,
      mojo::ScopedDataPipeProducerHandle send_stream);

  // Sets a global NetworkContext object to be used instead of the real one for
  // doing all network operations.
  static void SetNetworkContextForTesting(
      network::mojom::NetworkContext* network_context);

  static size_t GetNumInstances();

 private:
  enum SocketOption {
    SOCKET_OPTION_NODELAY = 1 << 0,
    SOCKET_OPTION_RCVBUF_SIZE = 1 << 1,
    SOCKET_OPTION_SNDBUF_SIZE = 1 << 2
  };

  ~PepperTCPSocketMessageFilter() override;

  void SetConnectedSocketOnUIThread(
      mojo::PendingRemote<network::mojom::TCPConnectedSocket> connected_socket,
      mojo::PendingReceiver<network::mojom::SocketObserver>
          socket_observer_receiver,
      mojo::ScopedDataPipeConsumerHandle receive_stream,
      mojo::ScopedDataPipeProducerHandle send_stream);

  // ppapi::host::ResourceMessageFilter overrides.
  void OnFilterDestroyed() override;
  scoped_refptr<base::SequencedTaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& message) override;
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  // BrowserPpapiHostImpl::InstanceObserver overrides.
  void OnHostDestroyed() override;

  // network::mojom::ResolveHostClient overrides.
  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const std::optional<net::AddressList>& resolved_addresses,
                  const std::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override;

  // network::mojom::SocketObserver overrides.
  void OnReadError(int net_error) override;
  void OnWriteError(int net_error) override;

  // Called either when the SocketObserver Mojo pipe has an error, or bad data
  // is received from it.
  void OnSocketObserverError();

  int32_t OnMsgBind(const ppapi::host::HostMessageContext* context,
                    const PP_NetAddress_Private& net_addr);
  int32_t OnMsgConnect(const ppapi::host::HostMessageContext* context,
                       const std::string& host,
                       uint16_t port);
  int32_t OnMsgConnectWithNetAddress(
      const ppapi::host::HostMessageContext* context,
      const PP_NetAddress_Private& net_addr);
  int32_t OnMsgSSLHandshake(
      const ppapi::host::HostMessageContext* context,
      const std::string& server_name,
      uint16_t server_port,
      const std::vector<std::vector<char>>& trusted_certs,
      const std::vector<std::vector<char>>& untrusted_certs);
  int32_t OnMsgRead(const ppapi::host::HostMessageContext* context,
                    int32_t bytes_to_read);
  int32_t OnMsgWrite(const ppapi::host::HostMessageContext* context,
                     const std::string& data);
  int32_t OnMsgListen(const ppapi::host::HostMessageContext* context,
                      int32_t backlog);
  int32_t OnMsgAccept(const ppapi::host::HostMessageContext* context);
  int32_t OnMsgClose(const ppapi::host::HostMessageContext* context);
  int32_t OnMsgSetOption(const ppapi::host::HostMessageContext* context,
                         PP_TCPSocket_Option name,
                         const ppapi::SocketOptionData& value);

  // Attempts to read up to |pending_read_size_| bytes from |receive_stream_|.
  // If any bytes are read, or there's an error, returns that information to
  // |pending_read_context_|.
  void TryRead();

  // Attempts to write |pending_write_data_| to |send_stream_|.
  // |pending_write_bytes_written_| reflects how much of the data has been
  // written to the stream so far. Once all bytes are written, or there's an
  // error, returns that information to |pending_write_context_|.
  void TryWrite();

  void OnResolveCompleted(
      int net_result,
      const std::optional<net::AddressList>& resolved_addresses);

  // Attempts to connect to all addresses in |address_list| in order.
  void StartConnect(const ppapi::host::ReplyMessageContext& context,
                    const net::AddressList& address_list);

  void OnConnectCompleted(const ppapi::host::ReplyMessageContext& context,
                          int net_result,
                          const std::optional<net::IPEndPoint>& local_addr,
                          const std::optional<net::IPEndPoint>& peer_addr,
                          mojo::ScopedDataPipeConsumerHandle receive_stream,
                          mojo::ScopedDataPipeProducerHandle send_stream);

  void OnSSLHandshakeCompleted(
      const ppapi::host::ReplyMessageContext& context,
      int net_result,
      mojo::ScopedDataPipeConsumerHandle receive_stream,
      mojo::ScopedDataPipeProducerHandle send_stream,
      const std::optional<net::SSLInfo>& ssl_info);

  void OnListenCompleted(const ppapi::host::ReplyMessageContext& context,
                         int net_result);

  void OnBindCompleted(const ppapi::host::ReplyMessageContext& context,
                       int net_result,
                       const std::optional<net::IPEndPoint>& local_addr);

  void OnAcceptCompleted(
      const ppapi::host::ReplyMessageContext& context,
      mojo::PendingReceiver<network::mojom::SocketObserver>
          socket_observer_receiver,
      int net_result,
      const std::optional<net::IPEndPoint>& remote_addr,
      mojo::PendingRemote<network::mojom::TCPConnectedSocket> connected_socket,
      mojo::ScopedDataPipeConsumerHandle receive_stream,
      mojo::ScopedDataPipeProducerHandle send_stream);

  void OnAcceptCompletedOnIOThread(
      const ppapi::host::ReplyMessageContext& context,
      mojo::PendingRemote<network::mojom::TCPConnectedSocket> connected_socket,
      mojo::PendingReceiver<network::mojom::SocketObserver>
          socket_observer_receiver,
      mojo::ScopedDataPipeConsumerHandle receive_stream,
      mojo::ScopedDataPipeProducerHandle send_stream,
      PP_NetAddress_Private pp_local_addr,
      PP_NetAddress_Private pp_remote_addr);

  // Sets the read/write streams and constructs watchers for them, which are not
  // armed until there's an attempt to use them that can't complete
  // synchronously.
  void SetStreams(mojo::ScopedDataPipeConsumerHandle receive_stream,
                  mojo::ScopedDataPipeProducerHandle send_stream);

#if BUILDFLAG(IS_CHROMEOS)
  void OpenFirewallHole(const ppapi::host::ReplyMessageContext& context);
  void OnFirewallHoleOpened(const ppapi::host::ReplyMessageContext& context,
                            std::unique_ptr<chromeos::FirewallHole> hole);
#endif  // BUILDFLAG(IS_CHROMEOS)

  void SendBindReply(const ppapi::host::ReplyMessageContext& context,
                     int32_t pp_result,
                     const PP_NetAddress_Private& local_addr);
  void SendBindError(const ppapi::host::ReplyMessageContext& context,
                     int32_t pp_error);
  void SendConnectReply(const ppapi::host::ReplyMessageContext& context,
                        int32_t pp_result,
                        const PP_NetAddress_Private& local_addr,
                        const PP_NetAddress_Private& remote_addr);
  void SendConnectError(const ppapi::host::ReplyMessageContext& context,
                        int32_t pp_error);
  void SendSSLHandshakeReply(const ppapi::host::ReplyMessageContext& context,
                             int32_t pp_result,
                             const std::optional<net::SSLInfo>& ssl_info);
  // The read and write reply messages use the |pending_*_context_| fields, and
  // clear fields related to the pending read / write request as needed.
  void SendReadReply(int32_t pp_result, const std::string& data);
  void SendReadError(int32_t pp_error);
  void SendWriteReply(int32_t pp_result);
  void SendListenReply(const ppapi::host::ReplyMessageContext& context,
                       int32_t pp_result);
  void SendAcceptReply(const ppapi::host::ReplyMessageContext& context,
                       int32_t pp_result,
                       int pending_host_id,
                       const PP_NetAddress_Private& local_addr,
                       const PP_NetAddress_Private& remote_addr);
  void SendAcceptError(const ppapi::host::ReplyMessageContext& context,
                       int32_t pp_error);

  // Closes any open Mojo pipe, and prevents new ones from being opened.
  void Close();

  network::mojom::NetworkContext* GetNetworkContext() const;

  bool IsPrivateAPI() const {
    return version_ == ppapi::TCP_SOCKET_VERSION_PRIVATE;
  }

  // These are used to create a callback that:
  // 1) if invoked with a network error code, will pass a message of the
  // requested type to |context| with the corresponding Pepper error.
  // 2) If destroyed without being invoked, will pass a message of the requested
  // type to |context| with PP_ERROR_FAILED.
  template <class ReturnMessage>
  base::OnceCallback<void(int net_result)> CreateCompletionCallback(
      const ppapi::host::HostMessageContext* context);
  template <class ReturnMessage>
  void ReturnResult(ppapi::host::ReplyMessageContext context, int net_result);

  // The following fields are used on both the UI and IO thread.
  const ppapi::TCPSocketVersion version_;

  // The following fields are used only on the IO thread.
  // Non-owning ptr.
  raw_ptr<BrowserPpapiHostImpl, DanglingUntriaged> host_;
  // Non-owning ptr.
  raw_ptr<ContentBrowserPepperHostFactory, AcrossTasksDanglingUntriaged>
      factory_;
  PP_Instance instance_;

  // The following fields are used only on the UI thread.
  const bool external_plugin_;

  int render_process_id_;
  int render_frame_id_;

  // A reference to |this| must always be taken while |receiver_| is bound to
  // ensure that if the error callback is called the object is alive.
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  mojo::Receiver<network::mojom::SocketObserver> socket_observer_receiver_{
      this};

  ppapi::TCPSocketState state_;

  // This is the address requested to bind. Please note that this is not the
  // bound address. For example, |bind_input_addr_| may have port set to 0.
  // It is used to check permission for listening.
  PP_NetAddress_Private bind_input_addr_;

  // The bound address.
  net::IPEndPoint bind_output_ip_endpoint_;

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<chromeos::FirewallHole> firewall_hole_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Bitwise-or of SocketOption flags. This stores the state about whether
  // each option is set before Connect() is called.
  int socket_options_;

  // Locally cached value of buffer size.
  int32_t rcvbuf_size_;
  int32_t sndbuf_size_;

  ppapi::host::ReplyMessageContext host_resolve_context_;

  // Holds socket if Bind() is called. Will be used to create a connected or
  // server socket, depending on the next call.
  mojo::Remote<network::mojom::TCPBoundSocket> bound_socket_;
  // Holds socket if Connect() is called.
  mojo::Remote<network::mojom::TCPConnectedSocket> connected_socket_;
  // Holds socket if socket was upgraded to SSL.
  mojo::Remote<network::mojom::TLSClientSocket> tls_client_socket_;
  // Holds socket if Listen() is called.
  mojo::Remote<network::mojom::TCPServerSocket> server_socket_;

  // Read/write pipes and their watchers. Both the watchers are configured so
  // that they must be armed to receive a notification.
  mojo::ScopedDataPipeConsumerHandle receive_stream_;
  std::unique_ptr<mojo::SimpleWatcher> read_watcher_;
  mojo::ScopedDataPipeProducerHandle send_stream_;
  std::unique_ptr<mojo::SimpleWatcher> write_watcher_;

  bool pending_accept_;

  size_t pending_read_size_;
  ppapi::host::ReplyMessageContext pending_read_context_;
  // This is set to an error other than PP_OK_COMPLETIONPENDING when a read
  // error is received through the SocketObserver interface. If the
  // SocketObserver interface is destroyed and this still hasn't been changed
  // from its initial value of PP_OK_COMPLETIONPENDING, it's set to
  // PP_ERROR_FAILED.
  int pending_read_pp_error_;

  std::string pending_write_data_;
  // Number of bytes from |pending_write_data_| that have already been written.
  // Always less than the size of |pending_write_data_|.
  size_t pending_write_bytes_written_;
  ppapi::host::ReplyMessageContext pending_write_context_;
  // This mirrors |pending_read_pp_error_|.
  int pending_write_pp_error_;

  const bool is_potentially_secure_plugin_context_;

  // Used in place of the StoragePartition's NetworkContext when non-null.
  static network::mojom::NetworkContext* network_context_for_testing;

  // Vends weak pointers on the UI thread, for callbacks passed through Mojo
  // pipes not owned by |this|. All weak pointers released in Close().
  base::WeakPtrFactory<PepperTCPSocketMessageFilter> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_TCP_SOCKET_MESSAGE_FILTER_H_
