// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_udp_socket_message_filter.h"

#include <cstring>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/browser/renderer_host/pepper/pepper_socket_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/process_type.h"
#include "content/public/common/socket_permission_request.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/error_conversion.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/udp_socket_resource_constants.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"
#include "ppapi/shared_impl/socket_option_data.h"
#include "services/network/public/mojom/network_context.mojom.h"

#if defined(OS_CHROMEOS)
#include "chromeos/network/firewall_hole.h"
#endif  // defined(OS_CHROMEOS)

using ppapi::NetAddressPrivateImpl;
using ppapi::host::NetErrorToPepperError;
using ppapi::proxy::UDPSocketResourceConstants;

namespace content {

namespace {

const PepperUDPSocketMessageFilter::CreateUDPSocketCallback*
    g_create_udp_socket_callback_for_testing = nullptr;

size_t g_num_udp_filter_instances = 0;

}  // namespace

PepperUDPSocketMessageFilter::PendingSend::PendingSend(
    const net::IPAddress& address,
    int port,
    std::vector<uint8_t> data,
    const ppapi::host::ReplyMessageContext& context)
    : address(address), port(port), data(std::move(data)), context(context) {}

PepperUDPSocketMessageFilter::PendingSend::PendingSend(
    const PendingSend& other) = default;

PepperUDPSocketMessageFilter::PendingSend::~PendingSend() {
}

PepperUDPSocketMessageFilter::PepperUDPSocketMessageFilter(
    BrowserPpapiHostImpl* host,
    PP_Instance instance,
    bool private_api)
    : socket_options_(0),
      rcvbuf_size_(0),
      sndbuf_size_(0),
      multicast_ttl_(0),
      can_use_multicast_(PP_ERROR_FAILED),
      closed_(false),
      remaining_recv_slots_(
          UDPSocketResourceConstants::kPluginReceiveBufferSlots),
      external_plugin_(host->external_plugin()),
      private_api_(private_api),
      render_process_id_(0),
      render_frame_id_(0),
      is_potentially_secure_plugin_context_(
          host->IsPotentiallySecurePluginContext(instance)) {
  ++g_num_udp_filter_instances;
  DCHECK(host);

  if (!host->GetRenderFrameIDsForInstance(
          instance, &render_process_id_, &render_frame_id_)) {
    NOTREACHED();
  }
}

PepperUDPSocketMessageFilter::~PepperUDPSocketMessageFilter() {
  DCHECK(closed_);
  DCHECK(!socket_);
  DCHECK(!receiver_.is_bound());
  --g_num_udp_filter_instances;
}

void PepperUDPSocketMessageFilter::SetCreateUDPSocketCallbackForTesting(
    const CreateUDPSocketCallback* create_udp_socket_callback) {
  DCHECK(!create_udp_socket_callback ||
         !g_create_udp_socket_callback_for_testing);
  g_create_udp_socket_callback_for_testing = create_udp_socket_callback;
}

// static
size_t PepperUDPSocketMessageFilter::GetNumInstances() {
  return g_num_udp_filter_instances;
}

void PepperUDPSocketMessageFilter::OnFilterDestroyed() {
  ResourceMessageFilter::OnFilterDestroyed();
  // Need to close the socket on the UI thread. Calling Close() also ensures
  // that future messages will be ignored, so the mojo pipes won't be
  // re-created, so after Close() runs, |this| can be safely deleted on the IO
  // thread.
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&PepperUDPSocketMessageFilter::Close, this));
}

scoped_refptr<base::TaskRunner>
PepperUDPSocketMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case PpapiHostMsg_UDPSocket_SetOption::ID:
    case PpapiHostMsg_UDPSocket_Close::ID:
    case PpapiHostMsg_UDPSocket_RecvSlotAvailable::ID:
    case PpapiHostMsg_UDPSocket_Bind::ID:
    case PpapiHostMsg_UDPSocket_SendTo::ID:
    case PpapiHostMsg_UDPSocket_JoinGroup::ID:
    case PpapiHostMsg_UDPSocket_LeaveGroup::ID:
      return base::CreateSingleThreadTaskRunner({BrowserThread::UI});
  }
  return nullptr;
}

int32_t PepperUDPSocketMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperUDPSocketMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_UDPSocket_SetOption,
                                      OnMsgSetOption)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_UDPSocket_Bind, OnMsgBind)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_UDPSocket_SendTo,
                                      OnMsgSendTo)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_UDPSocket_Close,
                                        OnMsgClose)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_UDPSocket_RecvSlotAvailable, OnMsgRecvSlotAvailable)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_UDPSocket_JoinGroup,
                                      OnMsgJoinGroup)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_UDPSocket_LeaveGroup,
                                      OnMsgLeaveGroup)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperUDPSocketMessageFilter::OnMsgSetOption(
    const ppapi::host::HostMessageContext* context,
    PP_UDPSocket_Option name,
    const ppapi::SocketOptionData& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (closed_)
    return PP_ERROR_FAILED;

  switch (name) {
    case PP_UDPSOCKET_OPTION_ADDRESS_REUSE: {
      if (socket_) {
        // AllowReuseAddress is only effective before Bind().
        // Note that this limitation originally comes from Windows, but
        // PPAPI tries to provide platform independent APIs.
        return PP_ERROR_FAILED;
      }

      bool boolean_value = false;
      if (!value.GetBool(&boolean_value))
        return PP_ERROR_BADARGUMENT;

      if (boolean_value) {
        socket_options_ |= SOCKET_OPTION_ADDRESS_REUSE;
      } else {
        socket_options_ &= ~SOCKET_OPTION_ADDRESS_REUSE;
      }
      return PP_OK;
    }
    case PP_UDPSOCKET_OPTION_BROADCAST: {
      bool boolean_value = false;
      if (!value.GetBool(&boolean_value))
        return PP_ERROR_BADARGUMENT;

      if (socket_) {
        socket_->SetBroadcast(
            boolean_value,
            CreateCompletionCallback<PpapiPluginMsg_UDPSocket_SetOptionReply>(
                context));
        return PP_OK_COMPLETIONPENDING;
      }

      if (boolean_value) {
        socket_options_ |= SOCKET_OPTION_BROADCAST;
      } else {
        socket_options_ &= ~SOCKET_OPTION_BROADCAST;
      }

      return PP_OK;
    }
    case PP_UDPSOCKET_OPTION_SEND_BUFFER_SIZE: {
      int32_t integer_value = 0;
      if (!value.GetInt32(&integer_value) || integer_value <= 0 ||
          integer_value > UDPSocketResourceConstants::kMaxSendBufferSize)
        return PP_ERROR_BADARGUMENT;

      socket_options_ |= SOCKET_OPTION_SNDBUF_SIZE;
      sndbuf_size_ = integer_value;

      // If the socket is already initialized, proxy the value to UDPSocket.
      if (socket_) {
        socket_->SetSendBufferSize(
            integer_value,
            CreateCompletionCallback<PpapiPluginMsg_UDPSocket_SetOptionReply>(
                context));
        return PP_OK_COMPLETIONPENDING;
      }

      return PP_OK;
    }
    case PP_UDPSOCKET_OPTION_RECV_BUFFER_SIZE: {
      int32_t integer_value = 0;
      if (!value.GetInt32(&integer_value) || integer_value <= 0 ||
          integer_value > UDPSocketResourceConstants::kMaxReceiveBufferSize)
        return PP_ERROR_BADARGUMENT;

      socket_options_ |= SOCKET_OPTION_RCVBUF_SIZE;
      rcvbuf_size_ = integer_value;

      // If the socket is already initialized, proxy the value to UDPSocket.
      if (socket_) {
        socket_->SetReceiveBufferSize(
            integer_value,
            CreateCompletionCallback<PpapiPluginMsg_UDPSocket_SetOptionReply>(
                context));
        return PP_OK_COMPLETIONPENDING;
      }

      return PP_OK;
    }
    case PP_UDPSOCKET_OPTION_MULTICAST_LOOP: {
      bool boolean_value = false;
      if (!value.GetBool(&boolean_value))
        return PP_ERROR_BADARGUMENT;

      if (boolean_value) {
        socket_options_ |= SOCKET_OPTION_MULTICAST_LOOP;
      } else {
        socket_options_ &= ~SOCKET_OPTION_MULTICAST_LOOP;
      }

      // If the socket is already initialized, either fail if permissions
      // disallow multicast, or lie and claim it succeeded, to maintain previous
      // behavior.
      if (socket_) {
        if (can_use_multicast_ != PP_OK)
          return can_use_multicast_;

        return PP_OK;
      }

      return PP_OK;
    }
    case PP_UDPSOCKET_OPTION_MULTICAST_TTL: {
      int32_t integer_value = 0;
      if (!value.GetInt32(&integer_value) ||
          integer_value < 0 || integer_value > 255)
        return PP_ERROR_BADARGUMENT;

      // UDPSocket instance is not yet created, so remember the value here.
      socket_options_ |= SOCKET_OPTION_MULTICAST_TTL;
      multicast_ttl_ = integer_value;

      // If the socket is already initialized, either fail if permissions
      // disallow multicast, or lie and claim it succeeded, to maintain previous
      // behavior.
      if (socket_) {
        if (can_use_multicast_ != PP_OK)
          return can_use_multicast_;

        return PP_OK;
      }

      return PP_OK;
    }
    default: {
      NOTREACHED();
      return PP_ERROR_BADARGUMENT;
    }
  }
}

int32_t PepperUDPSocketMessageFilter::OnMsgBind(
    const ppapi::host::HostMessageContext* context,
    const PP_NetAddress_Private& addr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context);

  if (closed_ || socket_)
    return PP_ERROR_FAILED;

  // Check for permissions to use multicast APIS. This check must be done while
  // on the UI thread, so we cache the value here to be used later on.
  PP_NetAddress_Private any_addr;
  NetAddressPrivateImpl::GetAnyAddress(PP_FALSE, &any_addr);
  can_use_multicast_ = CanUseMulticastAPI(any_addr);

  SocketPermissionRequest request =
      pepper_socket_utils::CreateSocketPermissionRequest(
          SocketPermissionRequest::UDP_BIND, addr);
  if (!pepper_socket_utils::CanUseSocketAPIs(external_plugin_,
                                             private_api_,
                                             &request,
                                             render_process_id_,
                                             render_frame_id_)) {
    return PP_ERROR_NOACCESS;
  }

  net::IPAddressBytes address;
  uint16_t port;
  if (!NetAddressPrivateImpl::NetAddressToIPEndPoint(addr, &address, &port))
    return PP_ERROR_ADDRESS_INVALID;
  net::IPEndPoint end_point(net::IPAddress(address), port);

  network::mojom::UDPSocketOptionsPtr udp_socket_options =
      network::mojom::UDPSocketOptions::New();
  udp_socket_options->allow_address_reuse =
      !!(socket_options_ & SOCKET_OPTION_ADDRESS_REUSE);
  udp_socket_options->allow_broadcast =
      !!(socket_options_ & SOCKET_OPTION_BROADCAST);
  if (socket_options_ & SOCKET_OPTION_SNDBUF_SIZE)
    udp_socket_options->send_buffer_size = sndbuf_size_;
  if (socket_options_ & SOCKET_OPTION_RCVBUF_SIZE)
    udp_socket_options->receive_buffer_size = rcvbuf_size_;

  if (socket_options_ & SOCKET_OPTION_MULTICAST_LOOP) {
    if (can_use_multicast_ != PP_OK) {
      // TODO(mmenke):  The above line implies |can_use_multicast_| is a PP
      // error code, but the next line implies it is a net error code. Fix that.
      return NetErrorToPepperError(can_use_multicast_);
    }
    // TODO(mmenke): This doesn't seem to be doing anything - this is the
    // default behavior.
    udp_socket_options->multicast_loopback_mode = true;
  }
  if (socket_options_ & SOCKET_OPTION_MULTICAST_TTL) {
    if (can_use_multicast_ != PP_OK) {
      // TODO(mmenke):  The above line implies |can_use_multicast_| is a PP
      // error code, but the next line implies it is a net error code. Fix that.
      return NetErrorToPepperError(can_use_multicast_);
    }

    udp_socket_options->multicast_time_to_live = multicast_ttl_;
  }

  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  // If the RenderFrameHost has been closed, just fail the request.
  if (!render_frame_host)
    return PP_ERROR_NOACCESS;

  mojo::PendingRemote<network::mojom::UDPSocketListener> udp_socket_listener;
  // Avoid binding the listener until the socket has been successfully Bound (in
  // a socket sense), to avoid providing read data to the caller until it has
  // been told that the socket was bound.
  mojo::PendingReceiver<network::mojom::UDPSocketListener> listener_receiver =
      udp_socket_listener.InitWithNewPipeAndPassReceiver();

  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  network::mojom::NetworkContext* network_context =
      BrowserContext::GetStoragePartition(site_instance->GetBrowserContext(),
                                          site_instance)
          ->GetNetworkContext();
  if (g_create_udp_socket_callback_for_testing) {
    g_create_udp_socket_callback_for_testing->Run(
        network_context, socket_.BindNewPipeAndPassReceiver(),
        std::move(udp_socket_listener));
  } else {
    network_context->CreateUDPSocket(socket_.BindNewPipeAndPassReceiver(),
                                     std::move(udp_socket_listener));
  }

  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  // Watch the socket for errors during the the Bind call.
  socket_.set_disconnect_handler(
      base::BindOnce(&PepperUDPSocketMessageFilter::SendBindError,
                     base::Unretained(this), reply_context, PP_ERROR_FAILED));

  // This is the actual socket Bind call (i.e., not a Mojo Bind call).
  socket_->Bind(end_point, std::move(udp_socket_options),
                base::BindOnce(&PepperUDPSocketMessageFilter::DoBindCallback,
                               base::Unretained(this),
                               std::move(listener_receiver), reply_context));

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperUDPSocketMessageFilter::OnMsgSendTo(
    const ppapi::host::HostMessageContext* context,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context);

  // Check |receiver_| instead of |socket_| because |receiver_| is only set
  // after the Bind() call completes.
  if (closed_ || !receiver_.is_bound())
    return PP_ERROR_FAILED;

  SocketPermissionRequest request =
      pepper_socket_utils::CreateSocketPermissionRequest(
          SocketPermissionRequest::UDP_SEND_TO, addr);
  if (!pepper_socket_utils::CanUseSocketAPIs(external_plugin_,
                                             private_api_,
                                             &request,
                                             render_process_id_,
                                             render_frame_id_)) {
    return PP_ERROR_NOACCESS;
  }

  // Make sure a malicious plugin can't queue up an unlimited number of buffers.
  size_t num_pending_sends = pending_sends_.size();
  if (num_pending_sends == UDPSocketResourceConstants::kPluginSendBufferSlots) {
    return PP_ERROR_FAILED;
  }

  size_t num_bytes = data.size();
  if (num_bytes == 0 ||
      num_bytes >
          static_cast<size_t>(UDPSocketResourceConstants::kMaxWriteSize)) {
    // Size of |data| is checked on the plugin side.
    NOTREACHED();
    return PP_ERROR_BADARGUMENT;
  }

  net::IPAddressBytes address;
  uint16_t port;
  if (!NetAddressPrivateImpl::NetAddressToIPEndPoint(addr, &address, &port)) {
    return PP_ERROR_ADDRESS_INVALID;
  }

  const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(data.data());
  std::vector<uint8_t> data_vector(data_ptr, data_ptr + num_bytes);

  pending_sends_.push(PendingSend(net::IPAddress(address), port,
                                  std::move(data_vector),
                                  context->MakeReplyMessageContext()));
  // Can only start the send if there isn't another send pending.
  if (num_pending_sends == 0)
    StartPendingSend();
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperUDPSocketMessageFilter::OnMsgClose(
    const ppapi::host::HostMessageContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Close();
  return PP_OK;
}

int32_t PepperUDPSocketMessageFilter::OnMsgRecvSlotAvailable(
    const ppapi::host::HostMessageContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (remaining_recv_slots_ <
      UDPSocketResourceConstants::kPluginReceiveBufferSlots) {
    // If the pipe was closed, but the consumer has not yet closed the UDP
    // socket, keep the read buffer filled with errors.
    if (!receiver_.is_bound()) {
      PepperUDPSocketMessageFilter::SendRecvFromError(PP_ERROR_FAILED);
      return PP_OK;
    }

    remaining_recv_slots_++;
    socket_->ReceiveMore(1);
  }

  return PP_OK;
}

int32_t PepperUDPSocketMessageFilter::OnMsgJoinGroup(
    const ppapi::host::HostMessageContext* context,
    const PP_NetAddress_Private& addr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int32_t ret = CanUseMulticastAPI(addr);
  if (ret != PP_OK)
    return ret;

  if (!socket_)
    return PP_ERROR_FAILED;

  net::IPAddressBytes group;
  uint16_t port;

  if (!NetAddressPrivateImpl::NetAddressToIPEndPoint(addr, &group, &port))
    return PP_ERROR_ADDRESS_INVALID;

  socket_->JoinGroup(
      net::IPAddress(group),
      CreateCompletionCallback<PpapiPluginMsg_UDPSocket_SetOptionReply>(
          context));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperUDPSocketMessageFilter::OnMsgLeaveGroup(
    const ppapi::host::HostMessageContext* context,
    const PP_NetAddress_Private& addr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int32_t ret = CanUseMulticastAPI(addr);
  if (ret != PP_OK)
    return ret;

  if (!socket_)
    return PP_ERROR_FAILED;

  net::IPAddressBytes group;
  uint16_t port;

  if (!NetAddressPrivateImpl::NetAddressToIPEndPoint(addr, &group, &port))
    return PP_ERROR_ADDRESS_INVALID;

  socket_->LeaveGroup(
      net::IPAddress(group),
      CreateCompletionCallback<PpapiPluginMsg_UDPSocket_LeaveGroupReply>(
          context));
  return PP_OK_COMPLETIONPENDING;
}

void PepperUDPSocketMessageFilter::DoBindCallback(
    mojo::PendingReceiver<network::mojom::UDPSocketListener> listener_receiver,
    const ppapi::host::ReplyMessageContext& context,
    int result,
    const base::Optional<net::IPEndPoint>& local_addr_out) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (result != net::OK) {
    SendBindError(context, NetErrorToPepperError(result));
    return;
  }

  PP_NetAddress_Private net_address = NetAddressPrivateImpl::kInvalidNetAddress;
  if (!local_addr_out || !NetAddressPrivateImpl::IPEndPointToNetAddress(
                             local_addr_out->address().bytes(),
                             local_addr_out->port(), &net_address)) {
    SendBindError(context, PP_ERROR_ADDRESS_INVALID);
    return;
  }

#if defined(OS_CHROMEOS)
  pepper_socket_utils::OpenUDPFirewallHole(
      *local_addr_out,
      base::BindRepeating(&PepperUDPSocketMessageFilter::OnFirewallHoleOpened,
                          firewall_hole_weak_ptr_factory_.GetWeakPtr(),
                          base::Passed(std::move(listener_receiver)), context,
                          net_address));
#else   // !defined(OS_CHROMEOS)
  OnBindComplete(std::move(listener_receiver), context, net_address);
#endif  // !defined(OS_CHROMEOS)
}

void PepperUDPSocketMessageFilter::OnBindComplete(
    mojo::PendingReceiver<network::mojom::UDPSocketListener> listener_receiver,
    const ppapi::host::ReplyMessageContext& context,
    const PP_NetAddress_Private& net_address) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(socket_);

  SendBindReply(context, PP_OK, net_address);

  receiver_.Bind(std::move(listener_receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &PepperUDPSocketMessageFilter::PipeClosed, base::Unretained(this)));
  socket_.set_disconnect_handler(base::BindOnce(
      &PepperUDPSocketMessageFilter::PipeClosed, base::Unretained(this)));
  socket_->ReceiveMore(UDPSocketResourceConstants::kPluginReceiveBufferSlots);
}

#if defined(OS_CHROMEOS)
void PepperUDPSocketMessageFilter::OnFirewallHoleOpened(
    mojo::PendingReceiver<network::mojom::UDPSocketListener> listener_receiver,
    const ppapi::host::ReplyMessageContext& context,
    const PP_NetAddress_Private& net_address,
    std::unique_ptr<chromeos::FirewallHole> hole) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  LOG_IF(WARNING, !hole.get()) << "Firewall hole could not be opened.";
  firewall_hole_.reset(hole.release());

  OnBindComplete(std::move(listener_receiver), context, net_address);
}
#endif  // defined(OS_CHROMEOS)

void PepperUDPSocketMessageFilter::StartPendingSend() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!pending_sends_.empty());
  DCHECK(socket_);

  const PendingSend& pending_send = pending_sends_.front();
  // See OnMsgRecvFrom() for the reason why we use base::Unretained(this)
  // when calling |socket_| methods.
  socket_->SendTo(
      net::IPEndPoint(pending_send.address, pending_send.port),
      pending_send.data, pepper_socket_utils::PepperUDPNetworkAnnotationTag(),
      base::BindOnce(&PepperUDPSocketMessageFilter::OnSendToCompleted,
                     base::Unretained(this)));
}

void PepperUDPSocketMessageFilter::Close() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  socket_.reset();
  receiver_.reset();
  closed_ = true;
}

void PepperUDPSocketMessageFilter::OnReceived(
    int result,
    const base::Optional<net::IPEndPoint>& src_addr,
    base::Optional<base::span<const uint8_t>> data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!closed_);

  int32_t pp_result = NetErrorToPepperError(result);

  // Convert IPEndPoint we get back from RecvFrom to a PP_NetAddress_Private
  // to send back.
  PP_NetAddress_Private addr = NetAddressPrivateImpl::kInvalidNetAddress;
  if (pp_result == PP_OK &&
      (!src_addr ||
       !NetAddressPrivateImpl::IPEndPointToNetAddress(
           src_addr->address().bytes(), src_addr->port(), &addr))) {
    pp_result = PP_ERROR_ADDRESS_INVALID;
  }

  if (pp_result == PP_OK) {
    std::string data_string;
    if (data) {
      data_string = std::string(reinterpret_cast<const char*>(data->data()),
                                data->size());
    }
    SendRecvFromResult(PP_OK, data_string, addr);
  } else {
    SendRecvFromError(pp_result);
  }

  // This should always be the case, but best to protect against a broken /
  // taken over network service.
  if (remaining_recv_slots_ > 0)
    remaining_recv_slots_--;
}

void PepperUDPSocketMessageFilter::OnSendToCompleted(int net_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FinishPendingSend(net_result);

  if (!pending_sends_.empty())
    StartPendingSend();
}

void PepperUDPSocketMessageFilter::FinishPendingSend(int net_result) {
  DCHECK(!pending_sends_.empty());
  const PendingSend& pending_send = pending_sends_.front();
  int32_t pp_result = NetErrorToPepperError(net_result);
  if (pp_result < 0) {
    SendSendToError(pending_send.context, pp_result);
  } else {
    // The cast should be safe because of the
    // UDPSocketResourceConstants::kMaxSendBufferSize before enqueuing the send.
    SendSendToReply(pending_send.context, PP_OK,
                    static_cast<int>(pending_send.data.size()));
  }

  pending_sends_.pop();
}

void PepperUDPSocketMessageFilter::SendBindReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result,
    const PP_NetAddress_Private& addr) {
  UMA_HISTOGRAM_BOOLEAN("Pepper.PluginContextSecurity.UDPBind",
                        is_potentially_secure_plugin_context_);

  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(result);
  SendReply(reply_context, PpapiPluginMsg_UDPSocket_BindReply(addr));
}

void PepperUDPSocketMessageFilter::SendRecvFromResult(
    int32_t result,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  // Unlike SendReply, which is safe to call on any thread, SendUnsolicitedReply
  // calls are only safe to make on the IO thread.
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &PepperUDPSocketMessageFilter::SendRecvFromResultOnIOThread, this,
          result, data, addr));
}

void PepperUDPSocketMessageFilter::SendRecvFromResultOnIOThread(
    int32_t result,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  if (resource_host()) {
    resource_host()->host()->SendUnsolicitedReply(
        resource_host()->pp_resource(),
        PpapiPluginMsg_UDPSocket_PushRecvResult(result, data, addr));
  }
}

void PepperUDPSocketMessageFilter::SendSendToReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result,
    int32_t bytes_written) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(result);
  SendReply(reply_context, PpapiPluginMsg_UDPSocket_SendToReply(bytes_written));
}

void PepperUDPSocketMessageFilter::SendBindError(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result) {
  socket_.reset();
#if defined(OS_CHROMEOS)
  // In the unlikely case that this is due to a Mojo error while trying to open
  // a hole in the firewall on ChromeOS, abandon opening a hole in the firewall.
  firewall_hole_weak_ptr_factory_.InvalidateWeakPtrs();
#endif  // !defined(OS_CHROMEOS)
  SendBindReply(context, result, NetAddressPrivateImpl::kInvalidNetAddress);
}

void PepperUDPSocketMessageFilter::SendRecvFromError(
    int32_t result) {
  SendRecvFromResult(result, std::string(),
                     NetAddressPrivateImpl::kInvalidNetAddress);
}

void PepperUDPSocketMessageFilter::SendSendToError(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result) {
  SendSendToReply(context, result, 0);
}

void PepperUDPSocketMessageFilter::PipeClosed() {
  Close();

  while (!pending_sends_.empty())
    FinishPendingSend(PP_ERROR_FAILED);

  // Any reads should fail, after a pipe error.
  while (remaining_recv_slots_ > 0) {
    --remaining_recv_slots_;
    SendRecvFromError(PP_ERROR_FAILED);
  }
}

int32_t PepperUDPSocketMessageFilter::CanUseMulticastAPI(
    const PP_NetAddress_Private& addr) {
  // Check for plugin permissions.
  SocketPermissionRequest request =
      pepper_socket_utils::CreateSocketPermissionRequest(
          SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP, addr);
  if (!pepper_socket_utils::CanUseSocketAPIs(external_plugin_,
                                             private_api_,
                                             &request,
                                             render_process_id_,
                                             render_frame_id_)) {
    return PP_ERROR_NOACCESS;
  }

  return PP_OK;
}

template <class ReturnMessage>
base::OnceCallback<void(int result)>
PepperUDPSocketMessageFilter::CreateCompletionCallback(
    const ppapi::host::HostMessageContext* context) {
  std::unique_ptr<int> result = std::make_unique<int>(net::ERR_FAILED);
  int* result_ptr = result.get();
  base::ScopedClosureRunner closure_runner(
      base::BindOnce(&PepperUDPSocketMessageFilter::ReturnResult<ReturnMessage>,
                     base::Unretained(this), context->MakeReplyMessageContext(),
                     std::move(result)));
  return base::BindOnce(
      [](base::ScopedClosureRunner closure_runner, int* result_ptr,
         int net_result) { *result_ptr = net_result; },
      std::move(closure_runner), result_ptr);
}

template <class ReturnMessage>
void PepperUDPSocketMessageFilter::ReturnResult(
    const ppapi::host::ReplyMessageContext& context,
    std::unique_ptr<int> result) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(NetErrorToPepperError(*result));
  SendReply(reply_context, ReturnMessage());
}

}  // namespace content
