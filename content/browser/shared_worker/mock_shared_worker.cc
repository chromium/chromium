// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/mock_shared_worker.h"

#include "content/common/url_loader_factory_bundle.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

template <typename T>
bool CheckEquality(const T& expected, const T& actual) {
  EXPECT_EQ(expected, actual);
  return expected == actual;
}

}  // namespace

MockSharedWorker::MockSharedWorker(mojom::SharedWorkerRequest request)
    : binding_(this, std::move(request)) {}

MockSharedWorker::~MockSharedWorker() = default;

bool MockSharedWorker::CheckReceivedConnect(int* connection_request_id,
                                            blink::MessagePortChannel* port) {
  if (connect_received_.empty())
    return false;
  if (connection_request_id)
    *connection_request_id = connect_received_.front().first;
  if (port)
    *port = connect_received_.front().second;
  connect_received_.pop();
  return true;
}

bool MockSharedWorker::CheckNotReceivedConnect() {
  return connect_received_.empty();
}

bool MockSharedWorker::CheckReceivedTerminate() {
  if (!terminate_received_)
    return false;
  terminate_received_ = false;
  return true;
}

void MockSharedWorker::Connect(int connection_request_id,
                               mojo::ScopedMessagePipeHandle port) {
  connect_received_.emplace(connection_request_id,
                            blink::MessagePortChannel(std::move(port)));
}

void MockSharedWorker::Terminate() {
  // Allow duplicate events.
  terminate_received_ = true;
}

void MockSharedWorker::BindDevToolsAgent(
    blink::mojom::DevToolsAgentHostAssociatedPtrInfo host_ptr_info,
    blink::mojom::DevToolsAgentAssociatedRequest request) {
  NOTREACHED();
}

MockSharedWorkerFactory::MockSharedWorkerFactory(
    mojom::SharedWorkerFactoryRequest request)
    : binding_(this, std::move(request)) {}

MockSharedWorkerFactory::~MockSharedWorkerFactory() = default;

bool MockSharedWorkerFactory::CheckReceivedCreateSharedWorker(
    const GURL& expected_url,
    const std::string& expected_name,
    blink::WebContentSecurityPolicyType expected_content_security_policy_type,
    mojom::SharedWorkerHostPtr* host,
    mojom::SharedWorkerRequest* request) {
  std::unique_ptr<CreateParams> create_params = std::move(create_params_);
  if (!create_params)
    return false;
  if (!CheckEquality(expected_url, create_params->info->url))
    return false;
  if (!CheckEquality(expected_name, create_params->info->name))
    return false;
  if (!CheckEquality(expected_content_security_policy_type,
                     create_params->info->content_security_policy_type))
    return false;
  if (!create_params->interface_provider)
    return false;
  *host = std::move(create_params->host);
  *request = std::move(create_params->request);
  return true;
}

void MockSharedWorkerFactory::CreateSharedWorker(
    mojom::SharedWorkerInfoPtr info,
    bool pause_on_start,
    const base::UnguessableToken& devtools_worker_token,
    const RendererPreferences& renderer_preferences,
    mojom::RendererPreferenceWatcherRequest preference_watcher_request,
    blink::mojom::WorkerContentSettingsProxyPtr content_settings,
    mojom::ServiceWorkerProviderInfoForSharedWorkerPtr
        service_worker_provider_info,
    int appcache_host_id,
    network::mojom::URLLoaderFactoryAssociatedPtrInfo
        main_script_loader_factory,
    blink::mojom::SharedWorkerMainScriptLoadParamsPtr main_sciript_load_params,
    std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loader_factories,
    mojom::ControllerServiceWorkerInfoPtr controller_info,
    mojom::SharedWorkerHostPtr host,
    mojom::SharedWorkerRequest request,
    service_manager::mojom::InterfaceProviderPtr interface_provider) {
  DCHECK(!create_params_);
  create_params_ = std::make_unique<CreateParams>();
  create_params_->info = std::move(info);
  create_params_->pause_on_start = pause_on_start;
  create_params_->content_settings = std::move(content_settings);
  create_params_->host = std::move(host);
  create_params_->request = std::move(request);
  create_params_->interface_provider = std::move(interface_provider);
}

MockSharedWorkerFactory::CreateParams::CreateParams() = default;

MockSharedWorkerFactory::CreateParams::~CreateParams() = default;

MockSharedWorkerClient::MockSharedWorkerClient() : binding_(this) {}

MockSharedWorkerClient::~MockSharedWorkerClient() = default;

void MockSharedWorkerClient::Bind(mojom::SharedWorkerClientRequest request) {
  binding_.Bind(std::move(request));
}

void MockSharedWorkerClient::Close() {
  binding_.Close();
}

bool MockSharedWorkerClient::CheckReceivedOnCreated() {
  if (!on_created_received_)
    return false;
  on_created_received_ = false;
  return true;
}

bool MockSharedWorkerClient::CheckReceivedOnConnected(
    std::set<blink::mojom::WebFeature> expected_used_features) {
  if (!on_connected_received_)
    return false;
  on_connected_received_ = false;
  if (!CheckEquality(expected_used_features, on_connected_features_))
    return false;
  return true;
}

bool MockSharedWorkerClient::CheckReceivedOnFeatureUsed(
    blink::mojom::WebFeature expected_feature) {
  if (!on_feature_used_received_)
    return false;
  on_feature_used_received_ = false;
  if (!CheckEquality(expected_feature, on_feature_used_feature_))
    return false;
  return true;
}

bool MockSharedWorkerClient::CheckNotReceivedOnFeatureUsed() {
  return !on_feature_used_received_;
}

bool MockSharedWorkerClient::CheckReceivedOnScriptLoadFailed() {
  if (!on_script_load_failed_)
    return false;
  on_script_load_failed_ = false;
  return true;
}

void MockSharedWorkerClient::OnCreated(
    blink::mojom::SharedWorkerCreationContextType creation_context_type) {
  DCHECK(!on_created_received_);
  on_created_received_ = true;
}

void MockSharedWorkerClient::OnConnected(
    const std::vector<blink::mojom::WebFeature>& features_used) {
  DCHECK(!on_connected_received_);
  on_connected_received_ = true;
  for (auto feature : features_used)
    on_connected_features_.insert(feature);
}

void MockSharedWorkerClient::OnScriptLoadFailed() {
  DCHECK(!on_script_load_failed_);
  on_script_load_failed_ = true;
}

void MockSharedWorkerClient::OnFeatureUsed(blink::mojom::WebFeature feature) {
  DCHECK(!on_feature_used_received_);
  on_feature_used_received_ = true;
  on_feature_used_feature_ = feature;
}

}  // namespace content
