// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network_service_client.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/threading/sequence_bound.h"
#include "base/unguessable_token.h"
#include "content/browser/browsing_data/clear_site_data_handler.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/loader/webrtc_connections_observer.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service_util.h"
#include "services/network/public/cpp/load_info_util.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif

#if defined(OS_MAC)
#include "base/task/current_thread.h"
#endif

namespace content {
namespace {

WebContents* GetWebContents(int process_id, int routing_id) {
  if (process_id != network::mojom::kBrowserProcessId) {
    return WebContentsImpl::FromRenderFrameHostID(process_id, routing_id);
  }
  return WebContents::FromFrameTreeNodeId(routing_id);
}

}  // namespace

NetworkServiceClient::NetworkServiceClient(
    mojo::PendingReceiver<network::mojom::NetworkServiceClient>
        network_service_client_receiver)
    : receiver_(this, std::move(network_service_client_receiver))
#if defined(OS_ANDROID)
      ,
      app_status_listener_(base::android::ApplicationStatusListener::New(
          base::BindRepeating(&NetworkServiceClient::OnApplicationStateChange,
                              base::Unretained(this))))
#endif
{

#if defined(OS_MAC)
  if (base::CurrentUIThread::IsSet())  // Not set in some unit tests.
    net::CertDatabase::GetInstance()->StartListeningForKeychainEvents();
#endif

  if (IsOutOfProcessNetworkService()) {
    net::CertDatabase::GetInstance()->AddObserver(this);
    memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
        FROM_HERE, base::BindRepeating(&NetworkServiceClient::OnMemoryPressure,
                                       base::Unretained(this)));

#if defined(OS_ANDROID)
    DCHECK(!net::NetworkChangeNotifier::CreateIfNeeded());
    GetNetworkService()->GetNetworkChangeManager(
        network_change_manager_.BindNewPipeAndPassReceiver());
    net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
    net::NetworkChangeNotifier::AddMaxBandwidthObserver(this);
    net::NetworkChangeNotifier::AddIPAddressObserver(this);
    net::NetworkChangeNotifier::AddDNSObserver(this);
#endif
  }

  webrtc_connections_observer_ =
      std::make_unique<content::WebRtcConnectionsObserver>(base::BindRepeating(
          &NetworkServiceClient::OnPeerToPeerConnectionsCountChange,
          base::Unretained(this)));
}

NetworkServiceClient::~NetworkServiceClient() {
  if (IsOutOfProcessNetworkService()) {
    net::CertDatabase::GetInstance()->RemoveObserver(this);
#if defined(OS_ANDROID)
    net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
    net::NetworkChangeNotifier::RemoveMaxBandwidthObserver(this);
    net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
    net::NetworkChangeNotifier::RemoveDNSObserver(this);
#endif
  }
}

void NetworkServiceClient::OnLoadingStateUpdate(
    std::vector<network::mojom::LoadInfoPtr> infos,
    OnLoadingStateUpdateCallback callback) {
  std::map<WebContents*, network::mojom::LoadInfo> info_map;

  for (auto& info : infos) {
    auto* web_contents = GetWebContents(info->process_id, info->routing_id);
    if (!web_contents)
      continue;

    auto existing = info_map.find(web_contents);
    if (existing == info_map.end() ||
        network::LoadInfoIsMoreInteresting(*info, existing->second)) {
      info_map[web_contents] = *info;
    }
  }

  for (const auto& load_info : info_map) {
    net::LoadStateWithParam load_state;
    load_state.state = static_cast<net::LoadState>(load_info.second.load_state);
    load_state.param = load_info.second.state_param;
    static_cast<WebContentsImpl*>(load_info.first)
        ->LoadStateChanged(load_info.second.host, load_state,
                           load_info.second.upload_position,
                           load_info.second.upload_size);
  }

  std::move(callback).Run();
}

void NetworkServiceClient::OnCertDBChanged() {
  GetNetworkService()->OnCertDBChanged();
}

void NetworkServiceClient::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  GetNetworkService()->OnMemoryPressure(memory_pressure_level);
}

void NetworkServiceClient::OnPeerToPeerConnectionsCountChange(uint32_t count) {
  GetNetworkService()->OnPeerToPeerConnectionsCountChange(count);
}

#if defined(OS_ANDROID)
void NetworkServiceClient::OnApplicationStateChange(
    base::android::ApplicationState state) {
  GetNetworkService()->OnApplicationStateChange(state);
}

void NetworkServiceClient::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  network_change_manager_->OnNetworkChanged(
      false /* dns_changed */, false /* ip_address_changed */,
      true /* connection_type_changed */, network::mojom::ConnectionType(type),
      false /* connection_subtype_changed */,
      network::mojom::ConnectionSubtype(
          net::NetworkChangeNotifier::GetConnectionSubtype()));
}

void NetworkServiceClient::OnMaxBandwidthChanged(
    double max_bandwidth_mbps,
    net::NetworkChangeNotifier::ConnectionType type) {
  // The connection subtype change will trigger a max bandwidth change in the
  // network service notifier.
  network_change_manager_->OnNetworkChanged(
      false /* dns_changed */, false /* ip_address_changed */,
      false /* connection_type_changed */, network::mojom::ConnectionType(type),
      true /* connection_subtype_changed */,
      network::mojom::ConnectionSubtype(
          net::NetworkChangeNotifier::GetConnectionSubtype()));
}

void NetworkServiceClient::OnIPAddressChanged() {
  network_change_manager_->OnNetworkChanged(
      false /* dns_changed */, true /* ip_address_changed */,
      false /* connection_type_changed */,
      network::mojom::ConnectionType(
          net::NetworkChangeNotifier::GetConnectionType()),
      false /* connection_subtype_changed */,
      network::mojom::ConnectionSubtype(
          net::NetworkChangeNotifier::GetConnectionSubtype()));
}

void NetworkServiceClient::OnDNSChanged() {
  network_change_manager_->OnNetworkChanged(
      true /* dns_changed */, false /* ip_address_changed */,
      false /* connection_type_changed */,
      network::mojom::ConnectionType(
          net::NetworkChangeNotifier::GetConnectionType()),
      false /* connection_subtype_changed */,
      network::mojom::ConnectionSubtype(
          net::NetworkChangeNotifier::GetConnectionSubtype()));
}
#endif

void NetworkServiceClient::OnDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {
  GetContentClient()->browser()->OnNetworkServiceDataUseUpdate(
      network_traffic_annotation_id_hash, recv_bytes, sent_bytes);
}

void NetworkServiceClient::OnRawRequest(
    int32_t process_id,
    int32_t routing_id,
    const std::string& devtools_request_id,
    const net::CookieAccessResultList& cookies_with_access_result,
    std::vector<network::mojom::HttpRawHeaderPairPtr> headers) {
  devtools_instrumentation::OnRequestWillBeSentExtraInfo(
      process_id, routing_id, devtools_request_id, cookies_with_access_result,
      headers);
}

void NetworkServiceClient::OnRawResponse(
    int32_t process_id,
    int32_t routing_id,
    const std::string& devtools_request_id,
    const net::CookieAndLineAccessResultList& cookies_with_access_result,
    std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
    const base::Optional<std::string>& raw_response_headers) {
  devtools_instrumentation::OnResponseReceivedExtraInfo(
      process_id, routing_id, devtools_request_id, cookies_with_access_result,
      headers, raw_response_headers);
}

void NetworkServiceClient::OnCorsPreflightRequest(
    int32_t process_id,
    int32_t render_frame_id,
    const base::UnguessableToken& devtools_request_id,
    const network::ResourceRequest& request,
    const GURL& initiator_url) {
  devtools_instrumentation::OnCorsPreflightRequest(
      process_id, render_frame_id, devtools_request_id, request, initiator_url);
}

void NetworkServiceClient::OnCorsPreflightResponse(
    int32_t process_id,
    int32_t render_frame_id,
    const base::UnguessableToken& devtools_request_id,
    const GURL& url,
    network::mojom::URLResponseHeadPtr head) {
  devtools_instrumentation::OnCorsPreflightResponse(
      process_id, render_frame_id, devtools_request_id, url, std::move(head));
}

void NetworkServiceClient::OnCorsPreflightRequestCompleted(
    int32_t process_id,
    int32_t render_frame_id,
    const base::UnguessableToken& devtools_request_id,
    const network::URLLoaderCompletionStatus& status) {
  devtools_instrumentation::OnCorsPreflightRequestCompleted(
      process_id, render_frame_id, devtools_request_id, status);
}

}  // namespace content
