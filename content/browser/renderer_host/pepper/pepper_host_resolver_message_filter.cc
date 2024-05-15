// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_host_resolver_message_filter.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/browser/renderer_host/pepper/pepper_socket_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/socket_permission_request.h"
#include "net/base/address_list.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/resolve_error_info.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_host_resolver_private.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/error_conversion.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"
#include "services/network/public/mojom/network_context.mojom.h"

using ppapi::host::NetErrorToPepperError;
using ppapi::host::ReplyMessageContext;

namespace content {

namespace {

void PrepareRequestInfo(const PP_HostResolver_Private_Hint& hint,
                        network::mojom::ResolveHostParameters* params) {
  switch (hint.family) {
    case PP_NETADDRESSFAMILY_PRIVATE_IPV4:
      params->dns_query_type = net::DnsQueryType::A;
      break;
    case PP_NETADDRESSFAMILY_PRIVATE_IPV6:
      params->dns_query_type = net::DnsQueryType::AAAA;
      break;
    default:
      params->dns_query_type = net::DnsQueryType::UNSPECIFIED;
  }

  if (hint.flags & PP_HOST_RESOLVER_PRIVATE_FLAGS_CANONNAME)
    params->include_canonical_name = true;
  if (hint.flags & PP_HOST_RESOLVER_PRIVATE_FLAGS_LOOPBACK_ONLY)
    params->loopback_only = true;
}

void CreateNetAddressListFromAddressList(
    const net::AddressList& list,
    std::vector<PP_NetAddress_Private>* net_address_list) {
  DCHECK(net_address_list);

  net_address_list->clear();
  net_address_list->reserve(list.size());

  PP_NetAddress_Private address;
  for (size_t i = 0; i < list.size(); ++i) {
    if (!ppapi::NetAddressPrivateImpl::IPEndPointToNetAddress(
            list[i].address().bytes(), list[i].port(), &address)) {
      net_address_list->clear();
      return;
    }
    net_address_list->push_back(address);
  }
}

}  // namespace

PepperHostResolverMessageFilter::PepperHostResolverMessageFilter(
    BrowserPpapiHostImpl* host,
    PP_Instance instance,
    bool private_api)
    : external_plugin_(host->external_plugin()),
      private_api_(private_api),
      render_process_id_(0),
      render_frame_id_(0) {
  DCHECK(host);

  if (!host->GetRenderFrameIDsForInstance(
          instance, &render_process_id_, &render_frame_id_)) {
    NOTREACHED_IN_MIGRATION();
  }
}

PepperHostResolverMessageFilter::~PepperHostResolverMessageFilter() {}

scoped_refptr<base::SequencedTaskRunner>
PepperHostResolverMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  if (message.type() == PpapiHostMsg_HostResolver_Resolve::ID)
    return GetUIThreadTaskRunner({});
  return nullptr;
}

int32_t PepperHostResolverMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperHostResolverMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_HostResolver_Resolve,
                                      OnMsgResolve)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperHostResolverMessageFilter::OnMsgResolve(
    const ppapi::host::HostMessageContext* context,
    const ppapi::HostPortPair& host_port,
    const PP_HostResolver_Private_Hint& hint) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check plugin permissions.
  SocketPermissionRequest request(
      SocketPermissionRequest::RESOLVE_HOST, host_port.host, host_port.port);
  if (!pepper_socket_utils::CanUseSocketAPIs(external_plugin_,
                                             private_api_,
                                             &request,
                                             render_process_id_,
                                             render_frame_id_)) {
    return PP_ERROR_NOACCESS;
  }

  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return PP_ERROR_FAILED;
  auto* storage_partition =
      render_frame_host->GetProcess()->GetStoragePartition();

  // Grab a reference to this class to ensure that it's fully alive if a
  // connection error occurs (i.e. ref count is higher than 0 and there's no
  // task from ResourceMessageFilterDeleteTraits to delete this object on the IO
  // thread pending). Balanced in OnComplete();
  AddRef();

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  PrepareRequestInfo(hint, parameters.get());

  // Intentionally using a HostPortPair because scheme isn't specified.
  storage_partition->GetNetworkContext()->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair(host_port.host, host_port.port)),
      render_frame_host->GetIsolationInfoForSubresources()
          .network_anonymization_key(),
      std::move(parameters), receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(base::BindOnce(
      &PepperHostResolverMessageFilter::OnComplete, base::Unretained(this),
      net::ERR_NAME_NOT_RESOLVED, net::ResolveErrorInfo(net::ERR_FAILED),
      /*resolved_addresses=*/std::nullopt,
      /*endpoint_results_with_metadata=*/std::nullopt));
  host_resolve_context_ = context->MakeReplyMessageContext();

  return PP_OK_COMPLETIONPENDING;
}

void PepperHostResolverMessageFilter::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const std::optional<net::AddressList>& resolved_addresses,
    const std::optional<net::HostResolverEndpointResults>&
        endpoint_results_with_metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  receiver_.reset();

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PepperHostResolverMessageFilter::OnLookupFinished, this,
                     resolve_error_info.error, std::move(resolved_addresses),
                     host_resolve_context_));
  host_resolve_context_ = ppapi::host::ReplyMessageContext();

  Release();  // Balances AddRef in OnMsgResolve.
}

void PepperHostResolverMessageFilter::OnLookupFinished(
    int net_result,
    const std::optional<net::AddressList>& addresses,
    const ReplyMessageContext& context) {
  if (net_result != net::OK) {
    SendResolveError(NetErrorToPepperError(net_result), context);
  } else {
    // Ignore DNS aliases unless only one is received. Otherwise unknown which
    // is the "canonical name" desired here. There is always expected to be at
    // most 1 alias when the request is made with `include_canonical_name` (see
    // `PrepareRequestInfo()`).
    const std::string& canonical_name =
        addresses.value().dns_aliases().size() == 1
            ? addresses.value().dns_aliases().front()
            : "";
    NetAddressList net_address_list;
    CreateNetAddressListFromAddressList(addresses.value(), &net_address_list);
    if (net_address_list.empty())
      SendResolveError(PP_ERROR_FAILED, context);
    else
      SendResolveReply(PP_OK, canonical_name, net_address_list, context);
  }
}

void PepperHostResolverMessageFilter::SendResolveReply(
    int32_t result,
    const std::string& canonical_name,
    const NetAddressList& net_address_list,
    const ReplyMessageContext& context) {
  ReplyMessageContext reply_context = context;
  reply_context.params.set_result(result);
  SendReply(reply_context,
            PpapiPluginMsg_HostResolver_ResolveReply(canonical_name,
                                                     net_address_list));
}

void PepperHostResolverMessageFilter::SendResolveError(
    int32_t error,
    const ReplyMessageContext& context) {
  SendResolveReply(error, std::string(), NetAddressList(), context);
}

}  // namespace content
