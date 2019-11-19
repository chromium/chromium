// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_

#include <memory>

#include "content/child/child_thread_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/service_worker/embedded_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_installed_scripts_manager.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom.h"
#include "third_party/blink/public/web/web_embedded_worker_start_data.h"

namespace content {

class ServiceWorkerContextClient;

// EmbeddedWorkerInstanceClientImpl is created in order to start a service
// worker. The browser processes sends a Mojo request that creates an instance
// of this class. The instance deletes itself when the service worker stops.  If
// the Mojo connection to the browser breaks first, the instance waits for the
// service worker to stop and then deletes itself.
//
// All methods are called on the thread that creates the instance of this class.
// Currently it's the main thread but it could be a background thread in the
// future. https://crbug.com/692909
class CONTENT_EXPORT EmbeddedWorkerInstanceClientImpl
    : public blink::mojom::EmbeddedWorkerInstanceClient {
 public:
  // Enum for UMA to record when StartWorker is received.
  enum class StartWorkerHistogramEnum {
    RECEIVED_ON_INSTALLED = 0,
    RECEIVED_ON_UNINSTALLED = 1,
    NUM_TYPES
  };

  // Creates a new EmbeddedWorkerInstanceClientImpl instance bound to
  // |receiver|. The instance destroys itself when needed, see the class
  // documentation.
  // TODO(shimazu): Create a service worker's execution context by this method
  // instead of just creating an instance of EmbeddedWorkerInstanceClient.
  static void Create(
      scoped_refptr<base::SingleThreadTaskRunner> initiator_task_runner,
      mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient>
          receiver);

  // TODO(https://crbug.com/955171): Remove this method and use Create once
  // RenderFrameHostImpl uses service_manager::BinderMap instead of
  // service_manager::BinderRegistry.
  static void CreateForRequest(
      scoped_refptr<base::SingleThreadTaskRunner> initiator_task_runner,
      blink::mojom::EmbeddedWorkerInstanceClientRequest request);

  ~EmbeddedWorkerInstanceClientImpl() override;

  // Destroys |this|. Called from ServiceWorkerContextClient.
  void WorkerContextDestroyed();

  // blink::mojom::EmbeddedWorkerInstanceClient implementation (partially
  // exposed to public)
  void StopWorker() override;

 private:
  friend class ServiceWorkerContextClientTest;

  EmbeddedWorkerInstanceClientImpl(
      mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient>
          receiver,
      scoped_refptr<base::SingleThreadTaskRunner> initiator_thread_task_runner);

  // blink::mojom::EmbeddedWorkerInstanceClient implementation
  void StartWorker(blink::mojom::EmbeddedWorkerStartParamsPtr params) override;
  void ResumeAfterDownload() override;

  // Handler of connection error bound to |receiver_|.
  void OnError();

  std::unique_ptr<blink::WebEmbeddedWorkerStartData> BuildStartData(
      const blink::mojom::EmbeddedWorkerStartParams& params);

  mojo::Receiver<blink::mojom::EmbeddedWorkerInstanceClient> receiver_;

  // A copy of this runner is also passed to ServiceWorkerContextClient in
  // StartWorker().
  scoped_refptr<base::SingleThreadTaskRunner> initiator_thread_task_runner_;

  // nullptr means worker is not running.
  std::unique_ptr<ServiceWorkerContextClient> service_worker_context_client_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstanceClientImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_
