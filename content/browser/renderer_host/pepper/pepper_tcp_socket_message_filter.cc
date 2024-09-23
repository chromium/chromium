// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_tcp_socket_message_filter.h"

#include <cstring>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/cstring_view.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/pepper/content_browser_pepper_host_factory.h"
#include "content/browser/renderer_host/pepper/pepper_socket_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/socket_permission_request.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/address_family.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/ssl/ssl_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/error_conversion.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/tcp_socket_resource_constants.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"
#include "ppapi/shared_impl/private/ppb_x509_util_shared.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/firewall_hole/firewall_hole.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using ppapi::NetAddressPrivateImpl;
using ppapi::TCPSocketState;
using ppapi::TCPSocketVersion;
using ppapi::host::NetErrorToPepperError;
using ppapi::proxy::TCPSocketResourceConstants;

namespace {

size_t g_num_tcp_filter_instances = 0;

}  // namespace

namespace content {

network::mojom::NetworkContext*
    PepperTCPSocketMessageFilter::network_context_for_testing = nullptr;

PepperTCPSocketMessageFilter::PepperTCPSocketMessageFilter(
    ContentBrowserPepperHostFactory* factory,
    BrowserPpapiHostImpl* host,
    PP_Instance instance,
    TCPSocketVersion version)
    : version_(version),
      host_(host),
      factory_(factory),
      instance_(instance),
      external_plugin_(host->external_plugin()),
      render_process_id_(0),
      render_frame_id_(0),
      state_(TCPSocketState::INITIAL),
      bind_input_addr_(NetAddressPrivateImpl::kInvalidNetAddress),
      socket_options_(SOCKET_OPTION_NODELAY),
      rcvbuf_size_(0),
      sndbuf_size_(0),
      pending_accept_(false),
      pending_read_size_(0),
      pending_read_pp_error_(PP_OK_COMPLETIONPENDING),
      pending_write_bytes_written_(0),
      pending_write_pp_error_(PP_OK_COMPLETIONPENDING),
      is_potentially_secure_plugin_context_(
          host->IsPotentiallySecurePluginContext(instance)) {
  DCHECK(host);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ++g_num_tcp_filter_instances;
  host_->AddInstanceObserver(instance_, this);
  if (!host->GetRenderFrameIDsForInstance(instance, &render_process_id_,
                                          &render_frame_id_)) {
    NOTREACHED_IN_MIGRATION();
  }
}

void PepperTCPSocketMessageFilter::SetConnectedSocket(
    mojo::PendingRemote<network::mojom::TCPConnectedSocket> connected_socket,
    mojo::PendingReceiver<network::mojom::SocketObserver>
        socket_observer_receiver,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // This method grabs a reference to |this|, and releases a reference on the UI
  // thread, so something should be holding on to a reference on the current
  // thread to prevent the object from being deleted before this method returns.
  DCHECK(HasOneRef());

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PepperTCPSocketMessageFilter::SetConnectedSocketOnUIThread, this,
          std::move(connected_socket), std::move(socket_observer_receiver),
          std::move(receive_stream), std::move(send_stream)));
}

PepperTCPSocketMessageFilter::~PepperTCPSocketMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (host_)
    host_->RemoveInstanceObserver(instance_, this);
  --g_num_tcp_filter_instances;
}

void PepperTCPSocketMessageFilter::SetConnectedSocketOnUIThread(
    mojo::PendingRemote<network::mojom::TCPConnectedSocket> connected_socket,
    mojo::PendingReceiver<network::mojom::SocketObserver>
        socket_observer_receiver,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(state_.state(), TCPSocketState::INITIAL);

  state_ = TCPSocketState(TCPSocketState::CONNECTED);
  connected_socket_.Bind(std::move(connected_socket));
  socket_observer_receiver_.Bind(std::move(socket_observer_receiver));
  socket_observer_receiver_.set_disconnect_handler(
      base::BindOnce(&PepperTCPSocketMessageFilter::OnSocketObserverError,
                     base::Unretained(this)));

  SetStreams(std::move(receive_stream), std::move(send_stream));
}

// static
void PepperTCPSocketMessageFilter::SetNetworkContextForTesting(
    network::mojom::NetworkContext* network_context) {
  network_context_for_testing = network_context;
}

// static
size_t PepperTCPSocketMessageFilter::GetNumInstances() {
  return g_num_tcp_filter_instances;
}

void PepperTCPSocketMessageFilter::OnFilterDestroyed() {
  ResourceMessageFilter::OnFilterDestroyed();
  // Need to close all mojo pipes the socket on the UI thread. Calling Close()
  // also ensures that future messages will be ignored, so the mojo pipes won't
  // be re-created, so after Close() runs, |this| can be safely deleted on the
  // IO thread.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PepperTCPSocketMessageFilter::Close, this));
}

scoped_refptr<base::SequencedTaskRunner>
PepperTCPSocketMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case PpapiHostMsg_TCPSocket_Bind::ID:
    case PpapiHostMsg_TCPSocket_Connect::ID:
    case PpapiHostMsg_TCPSocket_ConnectWithNetAddress::ID:
    case PpapiHostMsg_TCPSocket_Listen::ID:
    case PpapiHostMsg_TCPSocket_SSLHandshake::ID:
    case PpapiHostMsg_TCPSocket_Read::ID:
    case PpapiHostMsg_TCPSocket_Write::ID:
    case PpapiHostMsg_TCPSocket_Accept::ID:
    case PpapiHostMsg_TCPSocket_Close::ID:
    case PpapiHostMsg_TCPSocket_SetOption::ID:
      return GetUIThreadTaskRunner({});
  }
  return nullptr;
}

int32_t PepperTCPSocketMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperTCPSocketMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_TCPSocket_Bind, OnMsgBind)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_TCPSocket_Connect,
                                      OnMsgConnect)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_TCPSocket_ConnectWithNetAddress,
        OnMsgConnectWithNetAddress)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_TCPSocket_SSLHandshake,
                                      OnMsgSSLHandshake)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_TCPSocket_Read, OnMsgRead)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_TCPSocket_Write, OnMsgWrite)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_TCPSocket_Listen,
                                      OnMsgListen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_TCPSocket_Accept,
                                        OnMsgAccept)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_TCPSocket_Close,
                                        OnMsgClose)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_TCPSocket_SetOption,
                                      OnMsgSetOption)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

void PepperTCPSocketMessageFilter::OnHostDestroyed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  host_->RemoveInstanceObserver(instance_, this);
  host_ = nullptr;
}

void PepperTCPSocketMessageFilter::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const std::optional<net::AddressList>& resolved_addresses,
    const std::optional<net::HostResolverEndpointResults>&
        endpoint_results_with_metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  receiver_.reset();

  if (!host_resolve_context_.is_valid())
    return;

  ppapi::host::ReplyMessageContext context = host_resolve_context_;
  host_resolve_context_ = ppapi::host::ReplyMessageContext();

  if (!state_.IsPending(TCPSocketState::CONNECT)) {
    DCHECK(state_.state() == TCPSocketState::CLOSED);
    SendConnectError(context, PP_ERROR_FAILED);
    return;
  }

  if (result != net::OK) {
    SendConnectError(context, NetErrorToPepperError(resolve_error_info.error));
    state_.CompletePendingTransition(false);
    return;
  }

  StartConnect(context, resolved_addresses.value());
}

void PepperTCPSocketMessageFilter::OnReadError(int net_error) {
  // If this method is called more than once, or |net_error| isn't an allowed
  // value, just ignore the message.
  if (pending_read_pp_error_ != PP_OK_COMPLETIONPENDING || net_error > 0 ||
      net_error == net::ERR_IO_PENDING) {
    return;
  }

  pending_read_pp_error_ = NetErrorToPepperError(net_error);
  // Complete pending read with the error message if there's a pending read, and
  // the read data pipe has already been closed. If the pipe is still open, need
  // to wait until all data has been read before can start failing reads.
  if (pending_read_context_.is_valid() && !receive_stream_) {
    TryRead();
  }
}

void PepperTCPSocketMessageFilter::OnWriteError(int net_error) {
  // If this method is called more than once, or |net_error| isn't an allowed
  // value, just ignore the message.
  if (pending_write_pp_error_ != PP_OK_COMPLETIONPENDING || net_error > 0 ||
      net_error == net::ERR_IO_PENDING) {
    return;
  }

  pending_write_pp_error_ = NetErrorToPepperError(net_error);
  // Complete pending write with the error message if there's a pending write,
  // and the write data pipe has already been closed.
  if (pending_write_context_.is_valid() && !send_stream_)
    TryWrite();
}

void PepperTCPSocketMessageFilter::OnSocketObserverError() {
  // Note that this method may be called while a connection is still being made.
  socket_observer_receiver_.reset();

  // Treat this as a read and write error. If read and write errors have already
  // been received, these calls will do nothing.
  OnReadError(PP_ERROR_FAILED);
  OnWriteError(PP_ERROR_FAILED);
}

int32_t PepperTCPSocketMessageFilter::OnMsgBind(
    const ppapi::host::HostMessageContext* context,
    const PP_NetAddress_Private& net_addr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // This is only supported by PPB_TCPSocket v1.1 or above.
  if (version_ != ppapi::TCP_SOCKET_VERSION_1_1_OR_ABOVE) {
    NOTREACHED_IN_MIGRATION();
    return PP_ERROR_NOACCESS;
  }

  if (!pepper_socket_utils::CanUseSocketAPIs(
          external_plugin_, false /* private_api */, nullptr,
          render_process_id_, render_frame_id_)) {
    return PP_ERROR_NOACCESS;
  }

  if (state_.IsPending(TCPSocketState::BIND))
    return PP_ERROR_INPROGRESS;

  if (!state_.IsValidTransition(TCPSocketState::BIND))
    return PP_ERROR_FAILED;

  DCHECK(!bound_socket_);
  DCHECK(!connected_socket_);
  DCHECK(!server_socket_);

  // Validate address.
  net::IPAddressBytes address;
  uint16_t port;
  if (!NetAddressPrivateImpl::NetAddressToIPEndPoint(net_addr, &address,
                                                     &port)) {
    state_.DoTransition(TCPSocketState::BIND, false);
    return PP_ERROR_ADDRESS_INVALID;
  }

  network::mojom::NetworkContext* network_context = GetNetworkContext();
  if (!network_context)
    return PP_ERROR_FAILED;

  // The network service doesn't allow binding a socket without first
  // specifying if it's going to be used as a read or write socket,
  // so just hold onto the address for now, without actually binding anything.
  bind_input_addr_ = net_addr;

  state_.SetPendingTransition(TCPSocketState::BIND);
  network_context->CreateTCPBoundSocket(
      net::IPEndPoint(net::IPAddress(address), port),
      pepper_socket_utils::PepperTCPNetworkAnnotationTag(),
      bound_socket_.BindNewPipeAndPassReceiver(),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&PepperTCPSocketMessageFilter::OnBindCompleted,
                         weak_ptr_factory_.GetWeakPtr(),
                         context->MakeReplyMessageContext()),
          net::ERR_FAILED, std::nullopt /* local_addr */));

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTCPSocketMessageFilter::OnMsgConnect(
    const ppapi::host::HostMessageContext* context,
    const std::string& host,
    uint16_t port) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // This is only supported by PPB_TCPSocket_Private.
  if (!IsPrivateAPI()) {
    NOTREACHED_IN_MIGRATION();
    return PP_ERROR_NOACCESS;
  }

  SocketPermissionRequest request(SocketPermissionRequest::TCP_CONNECT, host,
                                  port);
  if (!pepper_socket_utils::CanUseSocketAPIs(
          external_plugin_, true /* private_api */, &request,
          render_process_id_, render_frame_id_)) {
    return PP_ERROR_NOACCESS;
  }

  if (!state_.IsValidTransition(TCPSocketState::CONNECT)) {
    NOTREACHED_IN_MIGRATION()
        << "This shouldn't be reached since the renderer only tries "
        << "to connect once.";
    return PP_ERROR_FAILED;
  }

  network::mojom::NetworkContext* network_context = GetNetworkContext();
  if (!network_context)
    return PP_ERROR_FAILED;

  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return PP_ERROR_FAILED;

  // Intentionally using a HostPortPair because scheme isn't specified.
  // TODO(mmenke): Pass in correct NetworkAnonymizationKey.
  network_context->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair(host, port)),
      render_frame_host->GetIsolationInfoForSubresources()
          .network_anonymization_key(),
      nullptr, receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(base::BindOnce(
      &PepperTCPSocketMessageFilter::OnComplete, base::Unretained(this),
      net::ERR_NAME_NOT_RESOLVED, net::ResolveErrorInfo(net::ERR_FAILED),
      /*resolved_addresses=*/std::nullopt,
      /*endpoint_results_with_metadata=*/std::nullopt));

  state_.SetPendingTransition(TCPSocketState::CONNECT);
  host_resolve_context_ = context->MakeReplyMessageContext();
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTCPSocketMessageFilter::OnMsgConnectWithNetAddress(
    const ppapi::host::HostMessageContext* context,
    const PP_NetAddress_Private& net_addr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  SocketPermissionRequest request =
      pepper_socket_utils::CreateSocketPermissionRequest(
          SocketPermissionRequest::TCP_CONNECT, net_addr);
  if (!pepper_socket_utils::CanUseSocketAPIs(external_plugin_, IsPrivateAPI(),
                                             &request, render_process_id_,
                                             render_frame_id_)) {
    return PP_ERROR_NOACCESS;
  }

  if (!state_.IsValidTransition(TCPSocketState::CONNECT))
    return PP_ERROR_FAILED;

  state_.SetPendingTransition(TCPSocketState::CONNECT);

  net::IPAddressBytes address;
  uint16_t port;
  if (!NetAddressPrivateImpl::NetAddressToIPEndPoint(net_addr, &address,
                                                     &port)) {
    state_.CompletePendingTransition(false);
    return PP_ERROR_ADDRESS_INVALID;
  }

  StartConnect(
      context->MakeReplyMessageContext(),
      net::AddressList(net::IPEndPoint(net::IPAddress(address), port)));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTCPSocketMessageFilter::OnMsgSSLHandshake(
    const ppapi::host::HostMessageContext* context,
    const std::string& server_name,
    uint16_t server_port,
    const std::vector<std::vector<char>>& trusted_certs,
    const std::vector<std::vector<char>>& untrusted_certs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Allow to do SSL handshake only if currently the socket has been connected
  // and there isn't pending read or write.
  if (!state_.IsValidTransition(TCPSocketState::SSL_CONNECT) ||
      pending_read_context_.is_valid() || pending_write_context_.is_valid()) {
    return PP_ERROR_FAILED;
  }

  // If there's a pending read or write error, fail the request with that.
  if (pending_read_pp_error_ != PP_OK_COMPLETIONPENDING) {
    if (pending_read_pp_error_ == PP_OK)
      pending_read_pp_error_ = PP_ERROR_FAILED;
    return pending_read_pp_error_;
  }

  if (pending_write_pp_error_ != PP_OK_COMPLETIONPENDING) {
    if (pending_write_pp_error_ == PP_OK)
      pending_write_pp_error_ = PP_ERROR_FAILED;
    return pending_write_pp_error_;
  }

  // TODO(raymes,rsleevi): Use trusted/untrusted certificates when connecting.

  // Close all Mojo pipes except |connected_socket_|.
  receive_stream_.reset();
  read_watcher_.reset();
  send_stream_.reset();
  write_watcher_.reset();
  socket_observer_receiver_.reset();

  state_.SetPendingTransition(TCPSocketState::SSL_CONNECT);

  network::mojom::TLSClientSocketOptionsPtr tls_client_socket_options =
      network::mojom::TLSClientSocketOptions::New();
  tls_client_socket_options->send_ssl_info = true;
  net::HostPortPair host_port_pair(server_name, server_port);
  connected_socket_->UpgradeToTLS(
      host_port_pair, std::move(tls_client_socket_options),
      pepper_socket_utils::PepperTCPNetworkAnnotationTag(),
      tls_client_socket_.BindNewPipeAndPassReceiver(),
      socket_observer_receiver_.BindNewPipeAndPassRemote(),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&PepperTCPSocketMessageFilter::OnSSLHandshakeCompleted,
                         base::Unretained(this),
                         context->MakeReplyMessageContext()),
          net::ERR_FAILED, mojo::ScopedDataPipeConsumerHandle(),
          mojo::ScopedDataPipeProducerHandle(), std::nullopt /* ssl_info */));

  socket_observer_receiver_.set_disconnect_handler(
      base::BindOnce(&PepperTCPSocketMessageFilter::OnSocketObserverError,
                     base::Unretained(this)));

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTCPSocketMessageFilter::OnMsgRead(
    const ppapi::host::HostMessageContext* context,
    int32_t bytes_to_read) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // This only covers the case where the socket was explicitly closed from the
  // caller, or the filter is being destroyed. Read errors and Mojo errors are
  // handled in TryRead().
  if (!state_.IsConnected())
    return PP_ERROR_FAILED;

  if (pending_read_context_.is_valid())
    return PP_ERROR_INPROGRESS;
  if (bytes_to_read <= 0 ||
      bytes_to_read > TCPSocketResourceConstants::kMaxReadSize) {
    return PP_ERROR_BADARGUMENT;
  }

  pending_read_context_ = context->MakeReplyMessageContext();
  pending_read_size_ = base::checked_cast<size_t>(bytes_to_read);
  TryRead();
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTCPSocketMessageFilter::OnMsgWrite(
    const ppapi::host::HostMessageContext* context,
    const std::string& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!state_.IsConnected())
    return PP_ERROR_FAILED;
  if (pending_write_context_.is_valid())
    return PP_ERROR_INPROGRESS;

  DCHECK(pending_write_data_.empty());
  DCHECK_EQ(0u, pending_write_bytes_written_);

  size_t data_size = data.size();
  if (data_size == 0 ||
      data_size >
          static_cast<size_t>(TCPSocketResourceConstants::kMaxWriteSize)) {
    return PP_ERROR_BADARGUMENT;
  }

  pending_write_data_ = data;
  pending_write_context_ = context->MakeReplyMessageContext();
  TryWrite();
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTCPSocketMessageFilter::OnMsgListen(
    const ppapi::host::HostMessageContext* context,
    int32_t backlog) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (state_.IsPending(TCPSocketState::LISTEN))
    return PP_ERROR_INPROGRESS;

  if (!state_.IsValidTransition(TCPSocketState::LISTEN))
    return PP_ERROR_FAILED;

  // This is only supported by PPB_TCPSocket v1.1 or above.
  if (version_ != ppapi::TCP_SOCKET_VERSION_1_1_OR_ABOVE) {
    NOTREACHED_IN_MIGRATION();
    return PP_ERROR_NOACCESS;
  }

  SocketPermissionRequest request =
      pepper_socket_utils::CreateSocketPermissionRequest(
          SocketPermissionRequest::TCP_LISTEN, bind_input_addr_);
  if (!pepper_socket_utils::CanUseSocketAPIs(
          external_plugin_, false /* private_api */, &request,
          render_process_id_, render_frame_id_)) {
    return PP_ERROR_NOACCESS;
  }

  DCHECK(bound_socket_);
  DCHECK(!server_socket_);

  state_.SetPendingTransition(TCPSocketState::LISTEN);

  bound_socket_->Listen(
      backlog, server_socket_.BindNewPipeAndPassReceiver(),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&PepperTCPSocketMessageFilter::OnListenCompleted,
                         base::Unretained(this),
                         context->MakeReplyMessageContext()),
          net::ERR_FAILED));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTCPSocketMessageFilter::OnMsgAccept(
    const ppapi::host::HostMessageContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (pending_accept_)
    return PP_ERROR_INPROGRESS;
  if (state_.state() != TCPSocketState::LISTENING)
    return PP_ERROR_FAILED;

  DCHECK(server_socket_);

  pending_accept_ = true;

  mojo::PendingRemote<network::mojom::SocketObserver> socket_observer;
  auto socket_observer_receiver =
      socket_observer.InitWithNewPipeAndPassReceiver();
  server_socket_->Accept(
      std::move(socket_observer),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&PepperTCPSocketMessageFilter::OnAcceptCompleted,
                         base::Unretained(this),
                         context->MakeReplyMessageContext(),
                         std::move(socket_observer_receiver)),
          net::ERR_FAILED, std::nullopt /* remote_addr */, mojo::NullRemote(),
          mojo::ScopedDataPipeConsumerHandle(),
          mojo::ScopedDataPipeProducerHandle()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTCPSocketMessageFilter::OnMsgClose(
    const ppapi::host::HostMessageContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (state_.state() == TCPSocketState::CLOSED)
    return PP_OK;

  Close();
  return PP_OK;
}

int32_t PepperTCPSocketMessageFilter::OnMsgSetOption(
    const ppapi::host::HostMessageContext* context,
    PP_TCPSocket_Option name,
    const ppapi::SocketOptionData& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Options are only applied if |this| is currently being used as a client
  // socket, or is going to be used as one - they are ignored for server
  // sockets.
  switch (name) {
    case PP_TCPSOCKET_OPTION_NO_DELAY: {
      bool boolean_value = false;
      if (!value.GetBool(&boolean_value))
        return PP_ERROR_BADARGUMENT;

      // If |connected_socket_| is connecting or has connected, pass the setting
      // along.
      if (connected_socket_.is_bound()) {
        connected_socket_->SetNoDelay(
            boolean_value,
            // Callback that converts a bool to a net error code which it then
            // passes to the common completion callback routine.
            base::BindOnce(
                [](base::OnceCallback<void(int)> completion_callback,
                   bool success) {
                  std::move(completion_callback)
                      .Run(success ? net::OK : net::ERR_FAILED);
                },
                CreateCompletionCallback<
                    PpapiPluginMsg_TCPSocket_SetOptionReply>(context)));
        return PP_OK_COMPLETIONPENDING;
      }

      // TCPConnectedSocket instance is not yet created. So remember the value
      // here.
      if (boolean_value) {
        socket_options_ |= SOCKET_OPTION_NODELAY;
      } else {
        socket_options_ &= ~SOCKET_OPTION_NODELAY;
      }
      return PP_OK;
    }
    case PP_TCPSOCKET_OPTION_SEND_BUFFER_SIZE: {
      int32_t integer_value = 0;
      if (!value.GetInt32(&integer_value) || integer_value <= 0 ||
          integer_value > TCPSocketResourceConstants::kMaxSendBufferSize)
        return PP_ERROR_BADARGUMENT;

      // If |connected_socket_| is connecting or has connected, pass the setting
      // along.
      if (connected_socket_.is_bound()) {
        connected_socket_->SetSendBufferSize(
            integer_value,
            CreateCompletionCallback<PpapiPluginMsg_TCPSocket_SetOptionReply>(
                context));
        return PP_OK_COMPLETIONPENDING;
      }

      // TCPSocket instance is not yet created. So remember the value here.
      socket_options_ |= SOCKET_OPTION_SNDBUF_SIZE;
      sndbuf_size_ = integer_value;
      return PP_OK;
    }
    case PP_TCPSOCKET_OPTION_RECV_BUFFER_SIZE: {
      int32_t integer_value = 0;
      if (!value.GetInt32(&integer_value) || integer_value <= 0 ||
          integer_value > TCPSocketResourceConstants::kMaxReceiveBufferSize)
        return PP_ERROR_BADARGUMENT;

      // If |connected_socket_| is connecting or has connected, pass the setting
      // along.
      if (connected_socket_.is_bound()) {
        connected_socket_->SetReceiveBufferSize(
            integer_value,
            CreateCompletionCallback<PpapiPluginMsg_TCPSocket_SetOptionReply>(
                context));
        return PP_OK_COMPLETIONPENDING;
      }

      // TCPConnectedSocket instance is not yet created. So remember the value
      // here.
      socket_options_ |= SOCKET_OPTION_RCVBUF_SIZE;
      rcvbuf_size_ = integer_value;
      return PP_OK;
    }
    default: {
      NOTREACHED_IN_MIGRATION();
      return PP_ERROR_BADARGUMENT;
    }
  }
}

void PepperTCPSocketMessageFilter::TryRead() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(state_.IsConnected());
  DCHECK(pending_read_context_.is_valid());
  DCHECK_GT(pending_read_size_, 0u);

  // This loop's body will generally run only once, unless there's a read error,
  // in which case, it will start over, to re-apply the initial logic.
  while (true) {
    // As long as the read stream is still open, try to read data, even if a
    // read error has been received from the SocketObserver, as there may still
    // be data on the pipe.
    if (!receive_stream_.is_valid()) {
      // If no read error has been received yet, wait to receive one through
      // the SocketObserver interface.
      if (pending_read_pp_error_ == PP_OK_COMPLETIONPENDING) {
        DCHECK(socket_observer_receiver_.is_bound());
        break;
      }

      // Otherwise, pass along the read error.
      SendReadError(pending_read_pp_error_);
      // If the socket was closed gracefully, only return OK for a single
      // read.
      if (pending_read_pp_error_ == PP_OK)
        pending_read_pp_error_ = PP_ERROR_FAILED;
      break;
    }

    DCHECK(read_watcher_);
    base::span<const uint8_t> buffer;
    int mojo_result =
        receive_stream_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
    if (mojo_result == MOJO_RESULT_SHOULD_WAIT) {
      read_watcher_->ArmOrNotify();
      break;
    }

    // On a Mojo pipe error (which may indicate a graceful close, network error,
    // or network service crash), close read pipe and restart the loop.
    if (mojo_result != MOJO_RESULT_OK) {
      read_watcher_.reset();
      receive_stream_.reset();
      continue;
    }

    // This is guaranteed by Mojo.
    DCHECK_GT(buffer.size(), 0u);

    std::string_view chars_to_copy = base::as_string_view(
        buffer.first(std::min(buffer.size(), pending_read_size_)));
    SendReadReply(PP_OK, std::string(chars_to_copy));
    receive_stream_->EndReadData(chars_to_copy.size());
    break;
  }
}

void PepperTCPSocketMessageFilter::TryWrite() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(state_.IsConnected());
  DCHECK(pending_write_context_.is_valid());
  DCHECK(!pending_write_data_.empty());
  DCHECK_LT(pending_write_bytes_written_, pending_write_data_.size());

  // The structure of this code largely mirrors TryRead() above, with a couple
  // differences. The loop will repeat until all bytes are written, there's an
  // error, or no more buffer space is available. Also, it's possible for a
  // Mojo write to succeed, but a write to the underlying socket to fail. In
  // that case, the failure might not be returned to the caller until it tries
  // to write again. Since the socket APIs themselves don't guarantee that
  // data has been successfully received by the remote server on success, this
  // should not cause unexpected problems for consumers.
  while (true) {
    if (!send_stream_.is_valid()) {
      if (pending_write_pp_error_ == PP_OK_COMPLETIONPENDING) {
        DCHECK(socket_observer_receiver_.is_bound());
        break;
      }
      SendWriteReply(pending_write_pp_error_);
      // Mirror handling of "OK" read errors, only sending "OK" for a single
      // write, though getting "OK" from a write is probably nonsense, anyways.
      if (pending_write_pp_error_ == PP_OK)
        pending_write_pp_error_ = PP_ERROR_FAILED;
      break;
    }

    DCHECK(write_watcher_);

    auto view = base::cstring_view(pending_write_data_);
    view.remove_prefix(pending_write_bytes_written_);
    DCHECK_GT(view.size(), 0u);
    size_t bytes_written = 0;
    int mojo_result = send_stream_->WriteData(
        base::as_byte_span(view), MOJO_WRITE_DATA_FLAG_NONE, bytes_written);
    if (mojo_result == MOJO_RESULT_SHOULD_WAIT) {
      write_watcher_->ArmOrNotify();
      break;
    }

    // On a Mojo pipe error (which may indicate a graceful close, network error,
    // or network service crash), close write pipe and restart the loop.
    if (mojo_result != MOJO_RESULT_OK) {
      write_watcher_.reset();
      send_stream_.reset();
      continue;
    }

    // This is guaranteed by Mojo.
    DCHECK_GT(bytes_written, 0u);

    pending_write_bytes_written_ += bytes_written;
    // If all bytes were written, nothing left to do.
    if (pending_write_bytes_written_ == pending_write_data_.size()) {
      SendWriteReply(pending_write_bytes_written_);
      break;
    }
  }
}

void PepperTCPSocketMessageFilter::StartConnect(
    const ppapi::host::ReplyMessageContext& context,
    const net::AddressList& address_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(state_.IsPending(TCPSocketState::CONNECT));
  DCHECK(!address_list.empty());

  auto socket_observer = socket_observer_receiver_.BindNewPipeAndPassRemote();
  socket_observer_receiver_.set_disconnect_handler(
      base::BindOnce(&PepperTCPSocketMessageFilter::OnSocketObserverError,
                     base::Unretained(this)));

  network::mojom::TCPConnectedSocketOptionsPtr socket_options =
      network::mojom::TCPConnectedSocketOptions::New();
  socket_options->no_delay = !!(socket_options_ & SOCKET_OPTION_NODELAY);
  if (socket_options_ & SOCKET_OPTION_RCVBUF_SIZE)
    socket_options->receive_buffer_size = rcvbuf_size_;
  if (socket_options_ & SOCKET_OPTION_SNDBUF_SIZE)
    socket_options->send_buffer_size = sndbuf_size_;

  network::mojom::NetworkContext::CreateTCPConnectedSocketCallback callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&PepperTCPSocketMessageFilter::OnConnectCompleted,
                         weak_ptr_factory_.GetWeakPtr(), context),
          net::ERR_FAILED, std::nullopt, std::nullopt,
          mojo::ScopedDataPipeConsumerHandle(),
          mojo::ScopedDataPipeProducerHandle());
  if (bound_socket_) {
    bound_socket_->Connect(address_list, std::move(socket_options),
                           connected_socket_.BindNewPipeAndPassReceiver(),
                           std::move(socket_observer), std::move(callback));
  } else {
    network::mojom::NetworkContext* network_context = GetNetworkContext();
    if (!network_context) {
      // This will delete |callback|, which will invoke OnConnectCompleted()
      // with an error.
      return;
    }
    network_context->CreateTCPConnectedSocket(
        std::nullopt /* local_addr */, address_list, std::move(socket_options),
        pepper_socket_utils::PepperTCPNetworkAnnotationTag(),
        connected_socket_.BindNewPipeAndPassReceiver(),
        std::move(socket_observer), std::move(callback));
  }
}

void PepperTCPSocketMessageFilter::OnConnectCompleted(
    const ppapi::host::ReplyMessageContext& context,
    int net_result,
    const std::optional<net::IPEndPoint>& local_addr,
    const std::optional<net::IPEndPoint>& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int32_t pp_result = NetErrorToPepperError(net_result);

  if (state_.state() == TCPSocketState::CLOSED) {
    // If this is called as a result of Close() being invoked and closing the
    // pipe, fail the request without doing anything.
    DCHECK_EQ(net_result, net::ERR_FAILED);
    SendConnectError(context, pp_result);
    return;
  }

  DCHECK(state_.IsPending(TCPSocketState::CONNECT));

  do {
    if (pp_result != PP_OK)
      break;

    PP_NetAddress_Private pp_local_addr =
        NetAddressPrivateImpl::kInvalidNetAddress;
    PP_NetAddress_Private pp_remote_addr =
        NetAddressPrivateImpl::kInvalidNetAddress;
    if (!local_addr || !peer_addr ||
        !NetAddressPrivateImpl::IPEndPointToNetAddress(
            local_addr->address().bytes(), local_addr->port(),
            &pp_local_addr) ||
        !NetAddressPrivateImpl::IPEndPointToNetAddress(
            peer_addr->address().bytes(), peer_addr->port(), &pp_remote_addr)) {
      pp_result = PP_ERROR_ADDRESS_INVALID;
      break;
    }

    SetStreams(std::move(receive_stream), std::move(send_stream));

    bound_socket_.reset();

    SendConnectReply(context, PP_OK, pp_local_addr, pp_remote_addr);
    state_.CompletePendingTransition(true);
    return;
  } while (false);

  // Handle errors.

  // This can happen even when the network service is behaving correctly, as
  // we may see the |socket_observer_receiver_| closed before receiving an
  // error.
  pending_read_pp_error_ = PP_OK_COMPLETIONPENDING;
  pending_write_pp_error_ = PP_OK_COMPLETIONPENDING;

  Close();
  SendConnectError(context, pp_result);

  if (version_ != ppapi::TCP_SOCKET_VERSION_1_1_OR_ABOVE) {
    // In order to maintain backward compatibility, allow further attempts
    // to connect the socket.
    state_ = TCPSocketState(TCPSocketState::INITIAL);
  }
}

void PepperTCPSocketMessageFilter::OnSSLHandshakeCompleted(
    const ppapi::host::ReplyMessageContext& context,
    int net_result,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream,
    const std::optional<net::SSLInfo>& ssl_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int pp_result = NetErrorToPepperError(net_result);
  if (state_.state() == TCPSocketState::CLOSED) {
    // If this is called as a result of Close() being invoked and closing the
    // pipe, fail the request without doing anything.
    DCHECK_EQ(net_result, net::ERR_FAILED);
    SendSSLHandshakeReply(context, pp_result, std::nullopt /* ssl_info */);
    return;
  }

  DCHECK(state_.IsPending(TCPSocketState::SSL_CONNECT));

  if (pp_result == PP_OK && !ssl_info)
    pp_result = PP_ERROR_FAILED;

  state_.CompletePendingTransition(pp_result == PP_OK);

  if (pp_result != PP_OK) {
    Close();
  } else {
    SetStreams(std::move(receive_stream), std::move(send_stream));
  }

  SendSSLHandshakeReply(context, pp_result, ssl_info);
}

void PepperTCPSocketMessageFilter::OnBindCompleted(
    const ppapi::host::ReplyMessageContext& context,
    int net_result,
    const std::optional<net::IPEndPoint>& local_addr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int pp_result = NetErrorToPepperError(net_result);
  if (state_.state() == TCPSocketState::CLOSED) {
    // If this is called as a result of Close() being invoked and closing the
    // pipe, fail the request without doing anything.
    DCHECK_EQ(net_result, net::ERR_FAILED);
    SendBindError(context, pp_result);
    return;
  }

  DCHECK(bound_socket_);
  DCHECK(state_.IsPending(TCPSocketState::BIND));

  PP_NetAddress_Private bound_address =
      NetAddressPrivateImpl::kInvalidNetAddress;
  if (pp_result == PP_OK &&
      (!local_addr || !NetAddressPrivateImpl::IPEndPointToNetAddress(
                          local_addr->address().bytes(), local_addr->port(),
                          &bound_address))) {
    pp_result = PP_ERROR_ADDRESS_INVALID;
  }

  if (pp_result != PP_OK) {
    bound_socket_.reset();
  } else {
    bind_output_ip_endpoint_ = *local_addr;
  }

  SendBindReply(context, pp_result, bound_address);
  state_.CompletePendingTransition(pp_result == PP_OK);
}

void PepperTCPSocketMessageFilter::OnListenCompleted(
    const ppapi::host::ReplyMessageContext& context,
    int net_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int pp_result = NetErrorToPepperError(net_result);
  if (state_.state() == TCPSocketState::CLOSED) {
    // If this is called as a result of Close() being invoked and closing the
    // pipe, fail the request without doing anything.
    DCHECK_EQ(net_result, net::ERR_FAILED);
    SendListenReply(context, pp_result);
    return;
  }

  DCHECK(state_.IsPending(TCPSocketState::LISTEN));

#if BUILDFLAG(IS_CHROMEOS)
  if (pp_result == PP_OK) {
    OpenFirewallHole(context);
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  SendListenReply(context, pp_result);
  state_.CompletePendingTransition(pp_result == PP_OK);
  if (pp_result != PP_OK)
    Close();
}

void PepperTCPSocketMessageFilter::OnAcceptCompleted(
    const ppapi::host::ReplyMessageContext& context,
    mojo::PendingReceiver<network::mojom::SocketObserver>
        socket_observer_receiver,
    int net_result,
    const std::optional<net::IPEndPoint>& remote_addr,
    mojo::PendingRemote<network::mojom::TCPConnectedSocket> connected_socket,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(pending_accept_);

  pending_accept_ = false;
  if (net_result != net::OK) {
    SendAcceptError(context, NetErrorToPepperError(net_result));
    return;
  }

  if (!remote_addr || !connected_socket.is_valid()) {
    SendAcceptError(context, NetErrorToPepperError(net_result));
    return;
  }

  DCHECK(socket_observer_receiver.is_valid());

  PP_NetAddress_Private pp_remote_addr =
      NetAddressPrivateImpl::kInvalidNetAddress;

  if (!NetAddressPrivateImpl::IPEndPointToNetAddress(
          remote_addr->address().bytes(), remote_addr->port(),
          &pp_remote_addr)) {
    SendAcceptError(context, PP_ERROR_ADDRESS_INVALID);
    return;
  }

  PP_NetAddress_Private bound_address =
      NetAddressPrivateImpl::kInvalidNetAddress;
  bool success = NetAddressPrivateImpl::IPEndPointToNetAddress(
      bind_output_ip_endpoint_.address().bytes(),
      bind_output_ip_endpoint_.port(), &bound_address);
  // This conversion should succeed, since it succeeded in OnBindComplete()
  // already.
  DCHECK(success);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PepperTCPSocketMessageFilter::OnAcceptCompletedOnIOThread,
                     this, context, std::move(connected_socket),
                     std::move(socket_observer_receiver),
                     std::move(receive_stream), std::move(send_stream),
                     bound_address, pp_remote_addr));
}

void PepperTCPSocketMessageFilter::OnAcceptCompletedOnIOThread(
    const ppapi::host::ReplyMessageContext& context,
    mojo::PendingRemote<network::mojom::TCPConnectedSocket> connected_socket,
    mojo::PendingReceiver<network::mojom::SocketObserver>
        socket_observer_receiver,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream,
    PP_NetAddress_Private pp_local_addr,
    PP_NetAddress_Private pp_remote_addr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!host_->IsValidInstance(instance_)) {
    // The instance has been removed while Accept was in progress. This object
    // should be destroyed and cleaned up after we release the reference we're
    // holding as a part of this function running so we just return without
    // doing anything.
    return;
  }

  // |factory_| is guaranteed to be non-NULL here. Only those instances created
  // in CONNECTED state have a NULL |factory_|, while getting here requires
  // LISTENING state.
  DCHECK(factory_);

  std::unique_ptr<ppapi::host::ResourceHost> host =
      factory_->CreateAcceptedTCPSocket(
          instance_, version_, std::move(connected_socket),
          std::move(socket_observer_receiver), std::move(receive_stream),
          std::move(send_stream));
  if (!host) {
    SendAcceptError(context, PP_ERROR_NOSPACE);
    return;
  }

  int pending_host_id =
      host_->GetPpapiHost()->AddPendingResourceHost(std::move(host));
  if (pending_host_id) {
    SendAcceptReply(context, PP_OK, pending_host_id, pp_local_addr,
                    pp_remote_addr);
  } else {
    SendAcceptError(context, PP_ERROR_NOSPACE);
  }
}

void PepperTCPSocketMessageFilter::SetStreams(
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK(!read_watcher_);
  DCHECK(!write_watcher_);

  receive_stream_ = std::move(receive_stream);
  read_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
  read_watcher_->Watch(
      receive_stream_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(
          [](PepperTCPSocketMessageFilter* message_filter,
             MojoResult /* result */,
             const mojo::HandleSignalsState& /* state */) {
            // TryRead will correctly handle both cases (data ready to be
            // read, and the pipe was closed).
            message_filter->TryRead();
          },
          base::Unretained(this)));

  send_stream_ = std::move(send_stream);
  write_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
  write_watcher_->Watch(
      send_stream_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(
          [](PepperTCPSocketMessageFilter* message_filter,
             MojoResult /* result */,
             const mojo::HandleSignalsState& /* state */) {
            // TryRead will correctly handle both cases (data ready to be
            // read, and the pipe was closed).
            message_filter->TryWrite();
          },
          base::Unretained(this)));
}

#if BUILDFLAG(IS_CHROMEOS)
void PepperTCPSocketMessageFilter::OpenFirewallHole(
    const ppapi::host::ReplyMessageContext& context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  pepper_socket_utils::OpenTCPFirewallHole(
      bind_output_ip_endpoint_,
      base::BindOnce(&PepperTCPSocketMessageFilter::OnFirewallHoleOpened,
                     weak_ptr_factory_.GetWeakPtr(), context));
}

void PepperTCPSocketMessageFilter::OnFirewallHoleOpened(
    const ppapi::host::ReplyMessageContext& context,
    std::unique_ptr<chromeos::FirewallHole> hole) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(state_.IsPending(TCPSocketState::LISTEN));
  LOG_IF(WARNING, !hole.get()) << "Firewall hole could not be opened.";
  firewall_hole_.reset(hole.release());

  SendListenReply(context, PP_OK);
  state_.CompletePendingTransition(true);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void PepperTCPSocketMessageFilter::SendBindReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_result,
    const PP_NetAddress_Private& local_addr) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(pp_result);
  SendReply(reply_context, PpapiPluginMsg_TCPSocket_BindReply(local_addr));
}

void PepperTCPSocketMessageFilter::SendBindError(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_error) {
  SendBindReply(context, pp_error, NetAddressPrivateImpl::kInvalidNetAddress);
}

void PepperTCPSocketMessageFilter::SendConnectReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_result,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(pp_result);
  SendReply(reply_context,
            PpapiPluginMsg_TCPSocket_ConnectReply(local_addr, remote_addr));
}

void PepperTCPSocketMessageFilter::SendConnectError(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_error) {
  SendConnectReply(context, pp_error, NetAddressPrivateImpl::kInvalidNetAddress,
                   NetAddressPrivateImpl::kInvalidNetAddress);
}

void PepperTCPSocketMessageFilter::SendSSLHandshakeReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_result,
    const std::optional<net::SSLInfo>& ssl_info) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(pp_result);
  ppapi::PPB_X509Certificate_Fields certificate_fields;
  if (pp_result == PP_OK) {
    DCHECK(ssl_info);
    if (ssl_info->cert.get()) {
      ppapi::PPB_X509Util_Shared::GetCertificateFields(*ssl_info->cert,
                                                       &certificate_fields);
    }
  }
  SendReply(reply_context,
            PpapiPluginMsg_TCPSocket_SSLHandshakeReply(certificate_fields));
}

void PepperTCPSocketMessageFilter::SendReadReply(int32_t pp_result,
                                                 const std::string& data) {
  DCHECK(pending_read_context_.is_valid());
  DCHECK_GT(pending_read_size_, 0u);

  pending_read_context_.params.set_result(pp_result);
  SendReply(pending_read_context_, PpapiPluginMsg_TCPSocket_ReadReply(data));

  pending_read_context_ = ppapi::host::ReplyMessageContext();
  pending_read_size_ = 0;
}

void PepperTCPSocketMessageFilter::SendReadError(int32_t pp_error) {
  SendReadReply(pp_error, std::string());
}

void PepperTCPSocketMessageFilter::SendWriteReply(int32_t pp_result) {
  DCHECK(pending_write_context_.is_valid());
  DCHECK(!pending_write_data_.empty());
  DCHECK(pp_result <= 0 ||
         static_cast<uint32_t>(pp_result) == pending_write_data_.size());
  DCHECK(pp_result <= 0 ||
         static_cast<uint32_t>(pp_result) == pending_write_bytes_written_);

  pending_write_context_.params.set_result(pp_result);
  SendReply(pending_write_context_, PpapiPluginMsg_TCPSocket_WriteReply());

  pending_write_context_ = ppapi::host::ReplyMessageContext();
  pending_write_data_.clear();
  pending_write_bytes_written_ = 0;
}

void PepperTCPSocketMessageFilter::SendListenReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_result) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(pp_result);
  SendReply(reply_context, PpapiPluginMsg_TCPSocket_ListenReply());
}

void PepperTCPSocketMessageFilter::SendAcceptReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_result,
    int pending_host_id,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(pp_result);
  SendReply(reply_context, PpapiPluginMsg_TCPSocket_AcceptReply(
                               pending_host_id, local_addr, remote_addr));
}

void PepperTCPSocketMessageFilter::SendAcceptError(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_error) {
  SendAcceptReply(context, pp_error, 0,
                  NetAddressPrivateImpl::kInvalidNetAddress,
                  NetAddressPrivateImpl::kInvalidNetAddress);
}

void PepperTCPSocketMessageFilter::Close() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Need to do these first, as destroying Mojo pipes may invoke some of the
  // callbacks with failure messages.
  weak_ptr_factory_.InvalidateWeakPtrs();
  state_.DoTransition(TCPSocketState::CLOSE, true);

#if BUILDFLAG(IS_CHROMEOS)
  // Close the firewall hole, it is no longer needed.
  firewall_hole_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Make sure there are no further callbacks from Mojo, which could end up in a
  // double free (Add ref on the UI thread, while a deletion is pending on the
  // IO thread), and that they're closed on the correct thread.
  bound_socket_.reset();
  connected_socket_.reset();
  tls_client_socket_.reset();
  server_socket_.reset();
  receiver_.reset();
  socket_observer_receiver_.reset();

  read_watcher_.reset();
  receive_stream_.reset();
  write_watcher_.reset();
  send_stream_.reset();
}

network::mojom::NetworkContext*
PepperTCPSocketMessageFilter::GetNetworkContext() const {
  if (network_context_for_testing)
    return network_context_for_testing;

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(render_process_id_);
  if (!render_process_host)
    return nullptr;

  return render_process_host->GetStoragePartition()->GetNetworkContext();
}

template <class ReturnMessage>
base::OnceCallback<void(int result)>
PepperTCPSocketMessageFilter::CreateCompletionCallback(
    const ppapi::host::HostMessageContext* context) {
  return mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&PepperTCPSocketMessageFilter::ReturnResult<ReturnMessage>,
                     base::Unretained(this),
                     context->MakeReplyMessageContext()),
      net::ERR_FAILED);
}

template <class ReturnMessage>
void PepperTCPSocketMessageFilter::ReturnResult(
    ppapi::host::ReplyMessageContext context,
    int net_result) {
  context.params.set_result(NetErrorToPepperError(net_result));
  SendReply(context, ReturnMessage());
}

}  // namespace content
