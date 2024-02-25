// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INSTALLED_SCRIPTS_SENDER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INSTALLED_SCRIPTS_SENDER_H_

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/service_worker/service_worker_installed_script_reader.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_installed_scripts_manager.mojom.h"

namespace content {

class ServiceWorkerVersion;

// ServiceWorkerInstalledScriptsSender serves the service worker's installed
// scripts from ServiceWorkerStorage to the renderer through Mojo data pipes.
// ServiceWorkerInstalledScriptsSender is owned by ServiceWorkerVersion. It is
// created for worker startup and lives as long as the worker is running.
//
// SWInstalledScriptsSender has three phases.
// 1. The sender sends all installed scripts to the renderer without any
//    requests from the renderer. This initial phase is called "streaming".
//    |state_| is kSendingScripts. When all installed scripts are sent, moves to
//    the phase 2.
// 2. The sender is idle. |state_| is kIdle. If the renderer calls
//    RequestInstalledScript, moves to the phase 3.
// 3. The sender sends requested scripts. |state_| is kSendingScripts. When all
//    the requested scripts are sent, returns to the phase 2.
class CONTENT_EXPORT ServiceWorkerInstalledScriptsSender
    : public blink::mojom::ServiceWorkerInstalledScriptsManagerHost,
      public ServiceWorkerInstalledScriptReader::Client {
 public:
  // |owner| must be an installed service worker.
  explicit ServiceWorkerInstalledScriptsSender(ServiceWorkerVersion* owner);

  ServiceWorkerInstalledScriptsSender(
      const ServiceWorkerInstalledScriptsSender&) = delete;
  ServiceWorkerInstalledScriptsSender& operator=(
      const ServiceWorkerInstalledScriptsSender&) = delete;

  ~ServiceWorkerInstalledScriptsSender() override;

  // Creates a Mojo struct (blink::mojom::ServiceWorkerInstalledScriptsInfo) and
  // sets it with the information to create
  // WebServiceWorkerInstalledScriptsManager on the renderer.
  blink::mojom::ServiceWorkerInstalledScriptsInfoPtr CreateInfoAndBind();

  // Starts sending installed scripts to the worker.
  void Start();

  // Returns the reason for the last time the sender entered the idle state. If
  // this sender has never reached the idle state, returns kNotFinished.
  ServiceWorkerInstalledScriptReader::FinishedReason last_finished_reason()
      const {
    return last_finished_reason_;
  }

  // Set a callback function to callback when all the update finished.
  void SetFinishCallback(base::OnceClosure callback);

 private:
  enum class State {
    kNotStarted,
    kSendingScripts,
    kIdle,
  };

  void StartSendingScript(int64_t resource_id, const GURL& script_url);

  // Stops all tasks even if pending scripts exist and disconnects the pipe to
  // the renderer. Also, if |reason| indicates failure to read the installed
  // script from the disk cache (kNoHTTPInfoError or kResponseReaderError), then
  // |owner_| is doomed via ServiceWorkerRegistration::DeleteVersion().
  void Abort(ServiceWorkerInstalledScriptReader::FinishedReason reason);

  void UpdateFinishedReasonAndBecomeIdle(
      ServiceWorkerInstalledScriptReader::FinishedReason reason);

  // Implements ServiceWorkerInstalledScriptReader::Client.
  void OnStarted(network::mojom::URLResponseHeadPtr response_head,
                 std::optional<mojo_base::BigBuffer> metadata,
                 mojo::ScopedDataPipeConsumerHandle body_handle,
                 mojo::ScopedDataPipeConsumerHandle meta_data_handle) override;
  void OnFinished(
      ServiceWorkerInstalledScriptReader::FinishedReason reason) override;

  // Implements blink::mojom::ServiceWorkerInstalledScriptsManagerHost.
  void RequestInstalledScript(const GURL& script_url) override;

  bool IsSendingMainScript() const;

  raw_ptr<ServiceWorkerVersion> owner_;
  const GURL main_script_url_;
  const int64_t main_script_id_;
  bool sent_main_script_;
  base::OnceClosure finish_callback_;

  mojo::Receiver<blink::mojom::ServiceWorkerInstalledScriptsManagerHost>
      receiver_{this};
  mojo::Remote<blink::mojom::ServiceWorkerInstalledScriptsManager> manager_;
  std::unique_ptr<ServiceWorkerInstalledScriptReader> reader_;

  State state_;
  ServiceWorkerInstalledScriptReader::FinishedReason last_finished_reason_;

  GURL current_sending_url_;
  base::queue<std::pair<int64_t /* resource_id */, GURL>> pending_scripts_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INSTALLED_SCRIPTS_SENDER_H_
