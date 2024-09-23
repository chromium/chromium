// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_network_proxy_host.h"

#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/browser/renderer_host/pepper/pepper_proxy_lookup_helper.h"
#include "content/browser/renderer_host/pepper/pepper_socket_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/socket_permission_request.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/network_isolation_key.h"
#include "net/proxy_resolution/proxy_info.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"

namespace content {

namespace {

bool LookUpProxyForURLCallback(
    int render_process_host_id,
    int render_frame_host_id,
    const GURL& url,
    mojo::PendingRemote<network::mojom::ProxyLookupClient>
        proxy_lookup_client) {
  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_host_id, render_frame_host_id);
  if (!render_frame_host)
    return false;

  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  StoragePartition* storage_partition =
      site_instance->GetBrowserContext()->GetStoragePartition(site_instance);

  storage_partition->GetNetworkContext()->LookUpProxyForURL(
      url,
      render_frame_host->GetIsolationInfoForSubresources()
          .network_anonymization_key(),
      std::move(proxy_lookup_client));
  return true;
}

}  // namespace

PepperNetworkProxyHost::PepperNetworkProxyHost(BrowserPpapiHostImpl* host,
                                               PP_Instance instance,
                                               PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      render_process_id_(0),
      render_frame_id_(0),
      is_allowed_(false),
      waiting_for_ui_thread_data_(true) {
  host->GetRenderFrameIDsForInstance(instance, &render_process_id_,
                                     &render_frame_id_);
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetUIThreadDataOnUIThread, render_process_id_,
                     render_frame_id_, host->external_plugin()),
      base::BindOnce(&PepperNetworkProxyHost::DidGetUIThreadData,
                     weak_factory_.GetWeakPtr()));
}

PepperNetworkProxyHost::~PepperNetworkProxyHost() = default;

PepperNetworkProxyHost::UIThreadData::UIThreadData() : is_allowed(false) {}

PepperNetworkProxyHost::UIThreadData::UIThreadData(const UIThreadData& other) =
    default;

PepperNetworkProxyHost::UIThreadData::~UIThreadData() {}

// static
PepperNetworkProxyHost::UIThreadData
PepperNetworkProxyHost::GetUIThreadDataOnUIThread(int render_process_id,
                                                  int render_frame_id,
                                                  bool is_external_plugin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PepperNetworkProxyHost::UIThreadData result;

  SocketPermissionRequest request(
      content::SocketPermissionRequest::RESOLVE_PROXY, std::string(), 0);
  result.is_allowed =
      pepper_socket_utils::CanUseSocketAPIs(is_external_plugin,
                                            false /* is_private_api */,
                                            &request,
                                            render_process_id,
                                            render_frame_id);
  return result;
}

void PepperNetworkProxyHost::DidGetUIThreadData(
    const UIThreadData& ui_thread_data) {
  is_allowed_ = ui_thread_data.is_allowed;
  waiting_for_ui_thread_data_ = false;
  TryToSendUnsentRequests();
}

int32_t PepperNetworkProxyHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperNetworkProxyHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_NetworkProxy_GetProxyForURL,
                                      OnMsgGetProxyForURL)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperNetworkProxyHost::OnMsgGetProxyForURL(
    ppapi::host::HostMessageContext* context,
    const std::string& url) {
  GURL gurl(url);
  if (gurl.is_valid()) {
    UnsentRequest request = {gurl, context->MakeReplyMessageContext()};
    unsent_requests_.push(request);
    TryToSendUnsentRequests();
  } else {
    SendFailureReply(PP_ERROR_BADARGUMENT, context->MakeReplyMessageContext());
  }
  return PP_OK_COMPLETIONPENDING;
}

void PepperNetworkProxyHost::TryToSendUnsentRequests() {
  if (waiting_for_ui_thread_data_)
    return;

  while (!unsent_requests_.empty()) {
    const UnsentRequest& request = unsent_requests_.front();
    if (!is_allowed_) {
      SendFailureReply(PP_ERROR_NOACCESS, request.reply_context);
    } else {
      // Everything looks valid, so try to resolve the proxy.
      auto lookup_helper = std::make_unique<PepperProxyLookupHelper>();
      PepperProxyLookupHelper::LookUpProxyForURLCallback
          look_up_proxy_for_url_callback = base::BindOnce(
              &LookUpProxyForURLCallback, render_process_id_, render_frame_id_);
      PepperProxyLookupHelper::LookUpCompleteCallback
          look_up_complete_callback =
              base::BindOnce(&PepperNetworkProxyHost::OnResolveProxyCompleted,
                             weak_factory_.GetWeakPtr(), request.reply_context,
                             lookup_helper.get());
      lookup_helper->Start(request.url,
                           std::move(look_up_proxy_for_url_callback),
                           std::move(look_up_complete_callback));
      pending_requests_.insert(std::move(lookup_helper));
    }
    unsent_requests_.pop();
  }
}

void PepperNetworkProxyHost::OnResolveProxyCompleted(
    ppapi::host::ReplyMessageContext context,
    PepperProxyLookupHelper* pending_request,
    std::optional<net::ProxyInfo> proxy_info) {
  auto it = pending_requests_.find(pending_request);
  CHECK(it != pending_requests_.end(), base::NotFatalUntil::M130);
  pending_requests_.erase(it);

  std::string pac_string;
  if (!proxy_info) {
    // This can happen in cases of network service crash, shutdown, or when
    // the request fails with ERR_MANDATORY_PROXY_CONFIGURATION_FAILED. There's
    // really no action a plugin can take, so there's no need to distinguish
    // which error occurred.
    context.params.set_result(PP_ERROR_FAILED);
  } else if (proxy_info->ContainsMultiProxyChain()) {
    // Multi-proxy chains cannot be represented as a PAC string.
    context.params.set_result(PP_ERROR_FAILED);
  } else {
    pac_string = proxy_info->ToPacString();
  }
  host()->SendReply(
      context, PpapiPluginMsg_NetworkProxy_GetProxyForURLReply(pac_string));
}

void PepperNetworkProxyHost::SendFailureReply(
    int32_t error,
    ppapi::host::ReplyMessageContext context) {
  context.params.set_result(error);
  host()->SendReply(
      context, PpapiPluginMsg_NetworkProxy_GetProxyForURLReply(std::string()));
}

}  // namespace content
