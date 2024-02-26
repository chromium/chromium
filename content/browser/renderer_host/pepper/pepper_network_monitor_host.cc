// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_network_monitor_host.h"

#include <stddef.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/browser/renderer_host/pepper/pepper_socket_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/socket_permission_request.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {

namespace {

bool CanUseNetworkMonitor(bool external_plugin,
                          int render_process_id,
                          int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  SocketPermissionRequest request = SocketPermissionRequest(
      SocketPermissionRequest::NETWORK_STATE, std::string(), 0);
  return pepper_socket_utils::CanUseSocketAPIs(external_plugin,
                                               false /* private_api */,
                                               &request,
                                               render_process_id,
                                               render_frame_id);
}

void OnGetNetworkList(
    base::OnceCallback<void(const net::NetworkInterfaceList&)> callback,
    const std::optional<net::NetworkInterfaceList>& networks) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), networks.has_value()
                                              ? *networks
                                              : net::NetworkInterfaceList()));
}

void GetNetworkList(
    base::OnceCallback<void(const net::NetworkInterfaceList&)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetNetworkService()->GetNetworkList(
      net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
      base::BindOnce(&OnGetNetworkList, std::move(callback)));
}

}  // namespace

PepperNetworkMonitorHost::PepperNetworkMonitorHost(BrowserPpapiHostImpl* host,
                                                   PP_Instance instance,
                                                   PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      network_connection_tracker_(nullptr) {
  int render_process_id;
  int render_frame_id;
  host->GetRenderFrameIDsForInstance(
      instance, &render_process_id, &render_frame_id);

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CanUseNetworkMonitor, host->external_plugin(),
                     render_process_id, render_frame_id),
      base::BindOnce(&PepperNetworkMonitorHost::OnPermissionCheckResult,
                     weak_factory_.GetWeakPtr()));
}

PepperNetworkMonitorHost::~PepperNetworkMonitorHost() {
  if (network_connection_tracker_)
    network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

void PepperNetworkMonitorHost::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  auto current_type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  network_connection_tracker_->GetConnectionType(&current_type,
                                                 base::DoNothing());
  if (type == current_type)
    GetAndSendNetworkList();
}

void PepperNetworkMonitorHost::OnPermissionCheckResult(
    bool can_use_network_monitor) {
  if (!can_use_network_monitor) {
    host()->SendUnsolicitedReply(pp_resource(),
                                 PpapiPluginMsg_NetworkMonitor_Forbidden());
    return;
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&content::GetNetworkConnectionTracker),
      base::BindOnce(&PepperNetworkMonitorHost::SetNetworkConnectionTracker,
                     weak_factory_.GetWeakPtr()));
  GetAndSendNetworkList();
}

void PepperNetworkMonitorHost::SetNetworkConnectionTracker(
    network::NetworkConnectionTracker* network_connection_tracker) {
  DCHECK_EQ(network_connection_tracker_, nullptr);
  network_connection_tracker_ = network_connection_tracker;
  network_connection_tracker_->AddNetworkConnectionObserver(this);
}

void PepperNetworkMonitorHost::GetAndSendNetworkList() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetNetworkList,
                     base::BindOnce(&PepperNetworkMonitorHost::SendNetworkList,
                                    weak_factory_.GetWeakPtr())));
}

void PepperNetworkMonitorHost::SendNetworkList(
    const net::NetworkInterfaceList& list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<ppapi::proxy::SerializedNetworkList> list_copy(
      new ppapi::proxy::SerializedNetworkList(list.size()));
  for (size_t i = 0; i < list.size(); ++i) {
    const net::NetworkInterface& network = list.at(i);
    ppapi::proxy::SerializedNetworkInfo& network_copy = list_copy->at(i);
    network_copy.name = network.name;

    network_copy.addresses.resize(
        1, ppapi::NetAddressPrivateImpl::kInvalidNetAddress);
    bool result = ppapi::NetAddressPrivateImpl::IPEndPointToNetAddress(
        network.address.bytes(), 0, &(network_copy.addresses[0]));
    DCHECK(result);

    // TODO(sergeyu): Currently net::NetworkInterfaceList provides
    // only name and one IP address. Add all other fields and copy
    // them here.
    network_copy.type = PP_NETWORKLIST_TYPE_UNKNOWN;
    network_copy.state = PP_NETWORKLIST_STATE_UP;
    network_copy.display_name = network.name;
    network_copy.mtu = 0;
  }
  host()->SendUnsolicitedReply(
      pp_resource(), PpapiPluginMsg_NetworkMonitor_NetworkList(*list_copy));
}

}  // namespace content
