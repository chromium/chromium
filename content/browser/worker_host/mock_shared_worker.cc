// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/mock_shared_worker.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

template <typename T>
bool CheckEquality(const T& expected, const T& actual) {
  EXPECT_EQ(expected, actual);
  return expected == actual;
}

}  // namespace

MockSharedWorker::MockSharedWorker(
    mojo::PendingReceiver<blink::mojom::SharedWorker> receiver)
    : receiver_(this, std::move(receiver)) {}

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

void MockSharedWorker::Disconnect() {
  receiver_.reset();
}

void MockSharedWorker::SetConnectCallback(base::OnceClosure callback) {
  connect_callback_ = std::move(callback);
}

void MockSharedWorker::Connect(int connection_request_id,
                               blink::MessagePortDescriptor port) {
  connect_received_.emplace(connection_request_id,
                            blink::MessagePortChannel(std::move(port)));
  if (connect_callback_) {
    std::move(connect_callback_).Run();
  }
}

void MockSharedWorker::Terminate() {
  // Allow duplicate events.
  terminate_received_ = true;
}

MockSharedWorkerFactory::MockSharedWorkerFactory(
    mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> receiver)
    : receiver_(this, std::move(receiver)) {}

MockSharedWorkerFactory::~MockSharedWorkerFactory() = default;

bool MockSharedWorkerFactory::CheckReceivedCreateSharedWorker(
    const GURL& expected_url,
    const std::string& expected_name,
    const std::vector<network::mojom::ContentSecurityPolicyPtr>&
        expected_content_security_policies,
    mojo::Remote<blink::mojom::SharedWorkerHost>* host,
    mojo::PendingReceiver<blink::mojom::SharedWorker>* receiver) {
  std::unique_ptr<CreateParams> create_params = std::move(create_params_);
  if (!create_params)
    return false;
  if (!CheckEquality(expected_url, create_params->info->url))
    return false;
  if (!CheckEquality(expected_name, create_params->info->options->name))
    return false;
  if (!CheckEquality(expected_content_security_policies,
                     create_params->info->content_security_policies)) {
    return false;
  }
  if (!CheckEquality(ukm::SourceIdType::WORKER_ID,
                     ukm::GetSourceIdType(create_params->ukm_source_id))) {
    return false;
  }
  host->Bind(std::move(create_params->host));
  *receiver = std::move(create_params->receiver);
  return true;
}

void MockSharedWorkerFactory::Disconnect() {
  receiver_.reset();
}

void MockSharedWorkerFactory::SetCreateWorkerCallback(
    base::OnceClosure callback) {
  create_worker_callback_ = std::move(callback);
}

void MockSharedWorkerFactory::CreateSharedWorker(
    blink::mojom::SharedWorkerInfoPtr info,
    const blink::SharedWorkerToken& token,
    const blink::StorageKey& constructor_key,
    const url::Origin& renderer_origin,
    bool is_constructor_secure_context,
    const std::string& user_agent,
    const blink::UserAgentMetadata& ua_metadata,
    bool pause_on_start,
    const base::UnguessableToken& devtools_worker_token,
    const blink::RendererPreferences& renderer_preferences,
    mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
        preference_watcher_receiver,
    mojo::PendingRemote<blink::mojom::WorkerContentSettingsProxy>
        content_settings,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr
        service_worker_container_info,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    blink::mojom::PolicyContainerPtr policy_container,
    mojo::PendingRemote<blink::mojom::SharedWorkerHost> host,
    mojo::PendingReceiver<blink::mojom::SharedWorker> receiver,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker,
    ukm::SourceId ukm_source_id,
    bool require_cross_site_request_for_cookies,
    mojo::PendingReceiver<blink::mojom::ReportingObserver>
        coep_reporting_observer,
    mojo::PendingReceiver<blink::mojom::ReportingObserver>
        dip_reporting_observer) {
  DCHECK(!create_params_);
  create_params_ = std::make_unique<CreateParams>();
  create_params_->info = std::move(info);
  create_params_->pause_on_start = pause_on_start;
  create_params_->content_settings = std::move(content_settings);
  create_params_->host = std::move(host);
  create_params_->receiver = std::move(receiver);
  create_params_->ukm_source_id = ukm_source_id;
  create_params_->require_cross_site_request_for_cookies =
      require_cross_site_request_for_cookies;
  if (create_worker_callback_) {
    std::move(create_worker_callback_).Run();
  }
}

MockSharedWorkerFactory::CreateParams::CreateParams() = default;

MockSharedWorkerFactory::CreateParams::~CreateParams() = default;

MockSharedWorkerClient::MockSharedWorkerClient() = default;

MockSharedWorkerClient::~MockSharedWorkerClient() = default;

void MockSharedWorkerClient::Bind(
    mojo::PendingReceiver<blink::mojom::SharedWorkerClient> receiver) {
  receiver_.Bind(std::move(receiver));
}

void MockSharedWorkerClient::Close() {
  receiver_.reset();
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

bool MockSharedWorkerClient::CheckReceivedOnReportException(
    blink::mojom::SharedWorkerExceptionDetailsPtr* details) {
  if (!on_report_exception_received_) {
    return false;
  }
  on_report_exception_received_ = false;
  *details = std::move(received_details_);
  return true;
}

void MockSharedWorkerClient::ResetReceiver() {
  receiver_.reset();
}

void MockSharedWorkerClient::SetOnReportExceptionCallback(
    base::OnceClosure callback) {
  on_report_exception_callback_ = std::move(callback);
}

void MockSharedWorkerClient::OnCreated(
    blink::mojom::SharedWorkerCreationContextType creation_context_type) {
  EXPECT_FALSE(on_created_received_);
  on_created_received_ = true;
}

void MockSharedWorkerClient::OnConnected(
    const std::vector<blink::mojom::WebFeature>& features_used) {
  EXPECT_FALSE(on_connected_received_);
  on_connected_received_ = true;
  for (auto feature : features_used)
    on_connected_features_.insert(feature);
}

void MockSharedWorkerClient::OnScriptLoadFailed(
    const std::string& error_message) {
  EXPECT_FALSE(on_script_load_failed_);
  on_script_load_failed_ = true;
}

void MockSharedWorkerClient::OnFeatureUsed(blink::mojom::WebFeature feature) {
  EXPECT_FALSE(on_feature_used_received_);
  on_feature_used_received_ = true;
  on_feature_used_feature_ = feature;
}

void MockSharedWorkerClient::OnReportException(
    blink::mojom::SharedWorkerExceptionDetailsPtr details) {
  EXPECT_FALSE(on_report_exception_received_);
  on_report_exception_received_ = true;
  received_details_ = std::move(details);
  if (on_report_exception_callback_) {
    std::move(on_report_exception_callback_).Run();
  }
}

}  // namespace content
