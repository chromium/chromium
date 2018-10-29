// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_

#include <memory>

#include "base/containers/id_map.h"
#include "base/single_thread_task_runner.h"
#include "content/child/child_thread_impl.h"
#include "content/common/service_worker/embedded_worker.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/common/privacy_preferences.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_installed_scripts_manager.mojom.h"
#include "third_party/blink/public/web/worker_content_settings_proxy.mojom.h"

namespace blink {

class WebEmbeddedWorker;

}  // namespace blink

namespace content {

class ServiceWorkerContextClient;

// This class exposes interfaces of WebEmbeddedWorker to the browser process.
// Unless otherwise noted, all methods should be called on the main thread.
// EmbeddedWorkerInstanceClientImpl is created in order to start a service
// worker, and lives as long as the service worker is running.
//
// This class deletes itself when the worker stops (or if start failed). The
// ownership graph is a cycle like this:
// EmbeddedWorkerInstanceClientImpl -(owns)-> WorkerWrapper -(owns)->
// WebEmbeddedWorkerImpl -(owns)-> ServiceWorkerContextClient -(owns)->
// EmbeddedWorkerInstanceClientImpl. Therefore, an instance can delete itself by
// releasing its WorkerWrapper.
//
// Since starting/stopping service workers is initiated by the browser process,
// the browser process effectively controls the lifetime of this class.
//
// TODO(shimazu): Let EmbeddedWorkerInstanceClientImpl own itself instead of
// the big reference cycle.
class EmbeddedWorkerInstanceClientImpl
    : public mojom::EmbeddedWorkerInstanceClient {
 public:
  // Enum for UMA to record when StartWorker is received.
  enum class StartWorkerHistogramEnum {
    RECEIVED_ON_INSTALLED = 0,
    RECEIVED_ON_UNINSTALLED = 1,
    NUM_TYPES
  };

  // Creates a new EmbeddedWorkerInstanceClientImpl instance bound to
  // |request|. The instance destroys itself when needed, see the class
  // documentation.
  // TODO(shimazu): Create a service worker's execution context by this method
  // instead of just creating an instance of EmbeddedWorkerInstanceClient.
  static void Create(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner,
      mojom::EmbeddedWorkerInstanceClientRequest request);

  ~EmbeddedWorkerInstanceClientImpl() override;

  // Called from ServiceWorkerContextClient.
  void WorkerContextDestroyed();

  // mojom::EmbeddedWorkerInstanceClient implementation (partially exposed to
  // public)
  void StopWorker() override;

 private:
  EmbeddedWorkerInstanceClientImpl(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner,
      mojom::EmbeddedWorkerInstanceClientRequest request);

  // mojom::EmbeddedWorkerInstanceClient implementation
  void StartWorker(mojom::EmbeddedWorkerStartParamsPtr params) override;
  void ResumeAfterDownload() override;
  void AddMessageToConsole(blink::WebConsoleMessage::Level level,
                           const std::string& message) override;
  void BindDevToolsAgent(
      blink::mojom::DevToolsAgentHostAssociatedPtrInfo host,
      blink::mojom::DevToolsAgentAssociatedRequest request) override;

  // Handler of connection error bound to |binding_|.
  void OnError();

  std::unique_ptr<blink::WebEmbeddedWorker> StartWorkerContext(
      mojom::EmbeddedWorkerStartParamsPtr params,
      std::unique_ptr<ServiceWorkerContextClient> context_client,
      blink::mojom::CacheStoragePtrInfo cache_storage,
      service_manager::mojom::InterfaceProviderPtrInfo interface_provider,
      blink::PrivacyPreferences privacy_preferences);

  mojo::Binding<mojom::EmbeddedWorkerInstanceClient> binding_;

  // This is valid before StartWorker is called. After that, this object
  // will be passed to ServiceWorkerContextClient.
  std::unique_ptr<EmbeddedWorkerInstanceClientImpl> temporal_self_;

  // nullptr means the worker is not running.
  std::unique_ptr<blink::WebEmbeddedWorker> worker_;

  scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstanceClientImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_
