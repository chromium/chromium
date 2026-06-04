// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webnn/webnn_introspection_manager_impl.h"

#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/webnn/webnn_introspection.mojom.h"

namespace content {

// static
WebNNIntrospectionManagerImpl* WebNNIntrospectionManagerImpl::GetInstance() {
  return base::Singleton<WebNNIntrospectionManagerImpl>::get();
}

WebNNIntrospectionManagerImpl::WebNNIntrospectionManagerImpl() = default;

WebNNIntrospectionManagerImpl::~WebNNIntrospectionManagerImpl() = default;

void WebNNIntrospectionManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WebNNIntrospectionManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
  // If there is no opened webnn-internals page to receive the recorded data,
  // disable graph recording to save resources.
  if (observers_.empty()) {
    SetMLGraphRecordEnabled(false);
  }
#endif
}

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
void WebNNIntrospectionManagerImpl::SetMLGraphRecordEnabled(bool enabled) {
  // Don't do anything if the enabled status hasn't actually changed.
  if (webnn_graph_record_enabled_ == enabled) {
    return;
  }

  webnn_graph_record_enabled_ = enabled;

  for (auto& observer : observers_) {
    observer.OnGraphRecordEnabledChanged(webnn_graph_record_enabled_);
  }

  if (enabled) {
    CHECK(introspection_renderer_remotes_.empty());
    CHECK(renderer_receivers_.empty());
    for (content::RenderProcessHost::iterator it(
             content::RenderProcessHost::AllHostsIterator());
         !it.IsAtEnd(); it.Advance()) {
      content::RenderProcessHost* host = it.GetCurrentValue();
      if (host->IsInitializedAndNotDead()) {
        ConfigWebNNIntrospectionForProcess(host);
      }
    }
    return;
  }

  // Disabling case
  introspection_renderer_remotes_.Clear();
  renderer_receivers_.Clear();
}

bool WebNNIntrospectionManagerImpl::IsMLGraphRecordEnabled() const {
  return webnn_graph_record_enabled_;
}

void WebNNIntrospectionManagerImpl::OnGraphRecorded(
    ::mojo_base::BigBuffer json_data) {
  for (auto& observer : observers_) {
    observer.OnGraphRecorded(json_data);
  }
}
#endif

void WebNNIntrospectionManagerImpl::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
  // For new created RenderProcessHost, we only need to configure WebNN
  // introspection if graph recording is enabled because the default state is
  // disabled.
  if (!webnn_graph_record_enabled_) {
    return;
  }
  ConfigWebNNIntrospectionForProcess(host);

#else
  // Avoid unused parameter warning when graph dump is disabled.
  std::ignore = host;
#endif
}

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
void WebNNIntrospectionManagerImpl::ConfigWebNNIntrospectionForProcess(
    content::RenderProcessHost* host) {
  CHECK(webnn_graph_record_enabled_);
  mojo::Remote<blink::mojom::WebNNIntrospection> debugger;
  host->BindReceiver(debugger.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<blink::mojom::WebNNIntrospectionClient> client;
  renderer_receivers_.Add(this, client.InitWithNewPipeAndPassReceiver());
  debugger->SetClient(std::move(client));

  introspection_renderer_remotes_.Add(std::move(debugger));
}
#endif

void WebNNIntrospectionManagerImpl::OnServiceDisconnect() {
  std::vector<webnn::mojom::WebNNContextIntrospectionDetailsPtr> empty_list;
  for (auto& observer : observers_) {
    observer.OnUpdateExistingContextDetails(empty_list);
  }
  service_receiver_.reset();
  introspection_service_remote_.reset();
}

void WebNNIntrospectionManagerImpl::OnUpdateExistingContextDetails(
    std::vector<webnn::mojom::WebNNContextIntrospectionDetailsPtr>
        contexts_details) {
  for (auto& observer : observers_) {
    observer.OnUpdateExistingContextDetails(contexts_details);
  }
}

void WebNNIntrospectionManagerImpl::OnUpdateAvailableExecutionProvidersDetails(
    std::vector<webnn::mojom::WebNNExecutionProviderDetailsPtr>
        available_execution_providers) {
  for (auto& observer : observers_) {
    observer.OnUpdateAvailableExecutionProvidersDetails(
        available_execution_providers);
  }
}

void WebNNIntrospectionManagerImpl::EnsureIntrospectionServiceConnection() {
  if (introspection_service_remote_.is_bound()) {
    return;
  }
  auto* host = content::GpuProcessHost::Get(content::GPU_PROCESS_KIND_SANDBOXED,
                                            /*force_create=*/false);
  // There may be no host nor service available. This may occur during
  // shutdown, when the service is fully disabled, and in some tests.
  // In those cases do nothing.
  if (!host) {
    return;
  }

  auto* gpu_service = host->gpu_service();
  if (!gpu_service) {
    return;
  }

  gpu_service->BindWebNNServiceIntrospection(
      introspection_service_remote_.BindNewPipeAndPassReceiver());
  mojo::PendingRemote<webnn::mojom::WebNNServiceIntrospectionClient> client;
  service_receiver_.Bind(client.InitWithNewPipeAndPassReceiver());
  introspection_service_remote_->SetClient(std::move(client));

  introspection_service_remote_.set_disconnect_handler(
      base::BindOnce(&WebNNIntrospectionManagerImpl::OnServiceDisconnect,
                     base::Unretained(this)));
}

void WebNNIntrospectionManagerImpl::
    EstablishServiceConnectionAndGetExistingContextsDetails(
        base::OnceCallback<void(
            std::vector<webnn::mojom::WebNNContextIntrospectionDetailsPtr>)>
            callback) {
  EnsureIntrospectionServiceConnection();
  if (!introspection_service_remote_.is_bound()) {
    // If the connection couldn't be established, return an empty list.
    std::move(callback).Run({});
    return;
  }
  introspection_service_remote_->GetExistingContextsDetails(
      std::move(callback));
}

void WebNNIntrospectionManagerImpl::
    EstablishServiceConnectionAndGetAvailableExecutionProvidersDetails(
        base::OnceCallback<
            void(std::vector<webnn::mojom::WebNNExecutionProviderDetailsPtr>)>
            callback) {
  EnsureIntrospectionServiceConnection();
  if (!introspection_service_remote_.is_bound()) {
    // If the connection couldn't be established, return an empty list.
    std::move(callback).Run({});
    return;
  }
  introspection_service_remote_->GetAvailableExecutionProvidersDetails(
      std::move(callback));
}

#if BUILDFLAG(IS_WIN)
void WebNNIntrospectionManagerImpl::ForceOrtEnvironmentCreationForIntrospection(
    base::OnceCallback<
        void(std::vector<webnn::mojom::WebNNExecutionProviderDetailsPtr>)>
        callback) {
  EnsureIntrospectionServiceConnection();
  if (!introspection_service_remote_.is_bound()) {
    std::move(callback).Run({});
    return;
  }
  introspection_service_remote_->ForceOrtEnvironmentCreationForIntrospection(
      std::move(callback));
}
#endif

}  // namespace content
