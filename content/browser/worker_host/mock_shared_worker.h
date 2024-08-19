// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_MOCK_SHARED_WORKER_H_
#define CONTENT_BROWSER_WORKER_HOST_MOCK_SHARED_WORKER_H_

#include <memory>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "content/browser/worker_host/shared_worker_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_factory.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom.h"

class GURL;

namespace blink {
class PendingURLLoaderFactoryBundle;
}  // namespace blink

namespace content {

class MockSharedWorker : public blink::mojom::SharedWorker {
 public:
  explicit MockSharedWorker(
      mojo::PendingReceiver<blink::mojom::SharedWorker> receiver);

  MockSharedWorker(const MockSharedWorker&) = delete;
  MockSharedWorker& operator=(const MockSharedWorker&) = delete;

  ~MockSharedWorker() override;

  bool CheckReceivedConnect(int* connection_request_id,
                            blink::MessagePortChannel* port);
  bool CheckNotReceivedConnect();
  bool CheckReceivedTerminate();

  void Disconnect();

 private:
  // blink::mojom::SharedWorker methods:
  void Connect(int connection_request_id,
               blink::MessagePortDescriptor port) override;
  void Terminate() override;

  mojo::Receiver<blink::mojom::SharedWorker> receiver_;
  std::queue<std::pair<int, blink::MessagePortChannel>> connect_received_;
  bool terminate_received_ = false;
};

class MockSharedWorkerFactory : public blink::mojom::SharedWorkerFactory {
 public:
  explicit MockSharedWorkerFactory(
      mojo::PendingReceiver<blink::mojom::SharedWorkerFactory> receiver);

  MockSharedWorkerFactory(const MockSharedWorkerFactory&) = delete;
  MockSharedWorkerFactory& operator=(const MockSharedWorkerFactory&) = delete;

  ~MockSharedWorkerFactory() override;

  bool CheckReceivedCreateSharedWorker(
      const GURL& expected_url,
      const std::string& expected_name,
      const std::vector<network::mojom::ContentSecurityPolicyPtr>&
          expected_content_security_policies,
      mojo::Remote<blink::mojom::SharedWorkerHost>* host,
      mojo::PendingReceiver<blink::mojom::SharedWorker>* receiver);

  void Disconnect();

 private:
  // blink::mojom::SharedWorkerFactory methods:
  void CreateSharedWorker(
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
      bool require_cross_site_request_for_cookies) override;

  struct CreateParams {
    CreateParams();
    ~CreateParams();
    blink::mojom::SharedWorkerInfoPtr info;
    bool pause_on_start;
    mojo::PendingRemote<blink::mojom::WorkerContentSettingsProxy>
        content_settings;
    mojo::PendingRemote<blink::mojom::SharedWorkerHost> host;
    mojo::PendingReceiver<blink::mojom::SharedWorker> receiver;
    ukm::SourceId ukm_source_id;
    bool require_cross_site_request_for_cookies;
  };

  mojo::Receiver<blink::mojom::SharedWorkerFactory> receiver_;
  std::unique_ptr<CreateParams> create_params_;
};

class MockSharedWorkerClient : public blink::mojom::SharedWorkerClient {
 public:
  MockSharedWorkerClient();

  MockSharedWorkerClient(const MockSharedWorkerClient&) = delete;
  MockSharedWorkerClient& operator=(const MockSharedWorkerClient&) = delete;

  ~MockSharedWorkerClient() override;

  void Bind(mojo::PendingReceiver<blink::mojom::SharedWorkerClient> receiver);
  void Close();
  bool CheckReceivedOnCreated();
  bool CheckReceivedOnConnected(
      std::set<blink::mojom::WebFeature> expected_used_features);
  bool CheckReceivedOnFeatureUsed(blink::mojom::WebFeature expected_feature);
  bool CheckNotReceivedOnFeatureUsed();
  bool CheckReceivedOnScriptLoadFailed();

  // Resets the receiver, allowing the caller to simulate losing the connection
  // with the client.
  void ResetReceiver();

 private:
  // blink::mojom::SharedWorkerClient methods:
  void OnCreated(blink::mojom::SharedWorkerCreationContextType
                     creation_context_type) override;
  void OnConnected(
      const std::vector<blink::mojom::WebFeature>& features_used) override;
  void OnScriptLoadFailed(const std::string& error_message) override;
  void OnFeatureUsed(blink::mojom::WebFeature feature) override;

  mojo::Receiver<blink::mojom::SharedWorkerClient> receiver_{this};
  bool on_created_received_ = false;
  bool on_connected_received_ = false;
  std::set<blink::mojom::WebFeature> on_connected_features_;
  bool on_feature_used_received_ = false;
  blink::mojom::WebFeature on_feature_used_feature_ =
      blink::mojom::WebFeature();
  bool on_script_load_failed_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_MOCK_SHARED_WORKER_H_
