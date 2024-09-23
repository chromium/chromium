// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "content/child/child_thread_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/service_worker/embedded_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_installed_scripts_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom-forward.h"
#include "third_party/blink/public/web/web_embedded_worker_start_data.h"

namespace content {

class ServiceWorkerContextClient;

// EmbeddedWorkerInstanceClientImpl is created in order to start a service
// worker. The browser processes sends a Mojo request that creates an instance
// of this class. The instance deletes itself when the service worker stops.  If
// the Mojo connection to the browser breaks first, the instance waits for the
// service worker to stop and then deletes itself.
//
// Created and lives on a ThreadPool background thread.
class EmbeddedWorkerInstanceClientImpl
    : public blink::mojom::EmbeddedWorkerInstanceClient {
 public:
  // Creates a new EmbeddedWorkerInstanceClientImpl instance bound to
  // |receiver|. The instance destroys itself when needed, see the class
  // documentation.
  // TODO(shimazu): Create a service worker's execution context by this method
  // instead of just creating an instance of EmbeddedWorkerInstanceClient.
  static void Create(
      scoped_refptr<base::SingleThreadTaskRunner> initiator_task_runner,
      const std::vector<std::string>& cors_exempt_header_list,
      mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient>
          receiver);

  EmbeddedWorkerInstanceClientImpl(const EmbeddedWorkerInstanceClientImpl&) =
      delete;
  EmbeddedWorkerInstanceClientImpl& operator=(
      const EmbeddedWorkerInstanceClientImpl&) = delete;

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
      scoped_refptr<base::SingleThreadTaskRunner> initiator_thread_task_runner,
      const std::vector<std::string>& cors_exempt_header_list);

  // blink::mojom::EmbeddedWorkerInstanceClient implementation
  void StartWorker(blink::mojom::EmbeddedWorkerStartParamsPtr params) override;

  // Handler of connection error bound to |receiver_|.
  void OnError();

  std::unique_ptr<blink::WebEmbeddedWorkerStartData> BuildStartData(
      const blink::mojom::EmbeddedWorkerStartParams& params);

  mojo::Receiver<blink::mojom::EmbeddedWorkerInstanceClient> receiver_;

  // A copy of this runner is also passed to ServiceWorkerContextClient in
  // StartWorker().
  scoped_refptr<base::SingleThreadTaskRunner> initiator_thread_task_runner_;

  std::vector<std::string> cors_exempt_header_list_;

  // nullptr means worker is not running.
  std::unique_ptr<ServiceWorkerContextClient> service_worker_context_client_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_CLIENT_IMPL_H_
