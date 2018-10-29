// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_MOCK_SHARED_WORKER_H_
#define CONTENT_BROWSER_SHARED_WORKER_MOCK_SHARED_WORKER_H_

#include <memory>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "content/browser/shared_worker/shared_worker_host.h"
#include "content/common/service_worker/service_worker_provider.mojom.h"
#include "content/common/shared_worker/shared_worker_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"

class GURL;

namespace content {
class URLLoaderFactoryBundleInfo;

class MockSharedWorker : public mojom::SharedWorker {
 public:
  explicit MockSharedWorker(mojom::SharedWorkerRequest request);
  ~MockSharedWorker() override;

  bool CheckReceivedConnect(int* connection_request_id,
                            blink::MessagePortChannel* port);
  bool CheckNotReceivedConnect();
  bool CheckReceivedTerminate();

 private:
  // mojom::SharedWorker methods:
  void Connect(int connection_request_id,
               mojo::ScopedMessagePipeHandle port) override;
  void Terminate() override;
  void BindDevToolsAgent(
      blink::mojom::DevToolsAgentHostAssociatedPtrInfo host_ptr_info,
      blink::mojom::DevToolsAgentAssociatedRequest request) override;

  mojo::Binding<mojom::SharedWorker> binding_;
  std::queue<std::pair<int, blink::MessagePortChannel>> connect_received_;
  bool terminate_received_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockSharedWorker);
};

class MockSharedWorkerFactory : public mojom::SharedWorkerFactory {
 public:
  explicit MockSharedWorkerFactory(mojom::SharedWorkerFactoryRequest request);
  ~MockSharedWorkerFactory() override;

  bool CheckReceivedCreateSharedWorker(
      const GURL& expected_url,
      const std::string& expected_name,
      blink::WebContentSecurityPolicyType expected_content_security_policy_type,
      mojom::SharedWorkerHostPtr* host,
      mojom::SharedWorkerRequest* request);

 private:
  // mojom::SharedWorkerFactory methods:
  void CreateSharedWorker(
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
      blink::mojom::SharedWorkerMainScriptLoadParamsPtr main_script_load_params,
      std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loader_factories,
      mojom::ControllerServiceWorkerInfoPtr controller_info,
      mojom::SharedWorkerHostPtr host,
      mojom::SharedWorkerRequest request,
      service_manager::mojom::InterfaceProviderPtr interface_provider) override;

  struct CreateParams {
    CreateParams();
    ~CreateParams();
    mojom::SharedWorkerInfoPtr info;
    bool pause_on_start;
    blink::mojom::WorkerContentSettingsProxyPtr content_settings;
    mojom::SharedWorkerHostPtr host;
    mojom::SharedWorkerRequest request;
    service_manager::mojom::InterfaceProviderPtr interface_provider;
  };

  mojo::Binding<mojom::SharedWorkerFactory> binding_;
  std::unique_ptr<CreateParams> create_params_;

  DISALLOW_COPY_AND_ASSIGN(MockSharedWorkerFactory);
};

class MockSharedWorkerClient : public mojom::SharedWorkerClient {
 public:
  MockSharedWorkerClient();
  ~MockSharedWorkerClient() override;

  void Bind(mojom::SharedWorkerClientRequest request);
  void Close();
  bool CheckReceivedOnCreated();
  bool CheckReceivedOnConnected(
      std::set<blink::mojom::WebFeature> expected_used_features);
  bool CheckReceivedOnFeatureUsed(blink::mojom::WebFeature expected_feature);
  bool CheckNotReceivedOnFeatureUsed();
  bool CheckReceivedOnScriptLoadFailed();

 private:
  // mojom::SharedWorkerClient methods:
  void OnCreated(blink::mojom::SharedWorkerCreationContextType
                     creation_context_type) override;
  void OnConnected(
      const std::vector<blink::mojom::WebFeature>& features_used) override;
  void OnScriptLoadFailed() override;
  void OnFeatureUsed(blink::mojom::WebFeature feature) override;

  mojo::Binding<mojom::SharedWorkerClient> binding_;
  bool on_created_received_ = false;
  bool on_connected_received_ = false;
  std::set<blink::mojom::WebFeature> on_connected_features_;
  bool on_feature_used_received_ = false;
  blink::mojom::WebFeature on_feature_used_feature_ =
      blink::mojom::WebFeature();
  bool on_script_load_failed_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockSharedWorkerClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_MOCK_SHARED_WORKER_H_
