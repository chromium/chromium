// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBNN_WEBNN_INTROSPECTION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_WEBNN_WEBNN_INTROSPECTION_MANAGER_IMPL_H_

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/webnn_introspection_manager.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/webnn/public/cpp/webnn_buildflags.h"
#include "services/webnn/public/mojom/webnn_service_introspection.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_service_introspection.mojom.h"
#include "third_party/blink/public/mojom/webnn/webnn_introspection.mojom.h"

namespace content {

class RenderProcessHost;

// A singleton that manages WebNN introspection and debugging capabilities
// across all renderer processes and also from the WebNN service in the GPU
// process.
//
// It observes RenderProcessHost creation to configure renderer-side
// introspection features, such as WebNN graph recording. As a central hub, it
// coordinates feature states and relays debugging data (like recorded graphs)
// from renderers to browser-side observers.
// Additionally, it connects to the WebNN service in the GPU process to provide
// additional introspection features.
// It is used only by chrome://webnn-internals.
class WebNNIntrospectionManagerImpl
    : public WebNNIntrospectionManager,
      public blink::mojom::WebNNIntrospectionClient,
      public webnn::mojom::WebNNServiceIntrospectionClient,
      public RenderProcessHostCreationObserver {
 public:
  static WebNNIntrospectionManagerImpl* GetInstance();

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void EstablishServiceConnectionAndGetExistingContextsDetails(
      base::OnceCallback<
          void(std::vector<webnn::mojom::WebNNContextIntrospectionDetailsPtr>)>
          callback) override;

  void EstablishServiceConnectionAndGetAvailableExecutionProvidersDetails(
      base::OnceCallback<
          void(std::vector<webnn::mojom::WebNNExecutionProviderDetailsPtr>)>
          callback) override;

#if BUILDFLAG(IS_WIN)
  void ForceOrtEnvironmentCreationForIntrospection(
      base::OnceCallback<
          void(std::vector<webnn::mojom::WebNNExecutionProviderDetailsPtr>)>
          callback) override;
#endif

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
  void SetMLGraphRecordEnabled(bool enabled) override;
  bool IsMLGraphRecordEnabled() const override;

  // blink::mojom::WebNNIntrospectionClient:
  void OnGraphRecorded(::mojo_base::BigBuffer json_data) override;
#endif

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

 private:
  friend struct base::DefaultSingletonTraits<WebNNIntrospectionManagerImpl>;
  WebNNIntrospectionManagerImpl();
  ~WebNNIntrospectionManagerImpl() override;

  void OnServiceDisconnect();
  // webnn::mojom::WebNNServiceIntrospectionClient:
  void OnUpdateExistingContextDetails(
      std::vector<webnn::mojom::WebNNContextIntrospectionDetailsPtr>
          contexts_details) override;
  void OnUpdateAvailableExecutionProvidersDetails(
      std::vector<webnn::mojom::WebNNExecutionProviderDetailsPtr>
          available_execution_providers) override;
  void EnsureIntrospectionServiceConnection();

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
  void ConfigWebNNIntrospectionForProcess(content::RenderProcessHost* host);

  bool webnn_graph_record_enabled_ = false;
#endif

  mojo::ReceiverSet<blink::mojom::WebNNIntrospectionClient> renderer_receivers_;
  base::ObserverList<Observer> observers_;
  mojo::RemoteSet<blink::mojom::WebNNIntrospection>
      introspection_renderer_remotes_;
  mojo::Receiver<webnn::mojom::WebNNServiceIntrospectionClient>
      service_receiver_{this};
  mojo::Remote<webnn::mojom::WebNNServiceIntrospection>
      introspection_service_remote_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBNN_WEBNN_INTROSPECTION_MANAGER_IMPL_H_
