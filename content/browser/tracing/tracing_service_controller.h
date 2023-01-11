// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACING_SERVICE_CONTROLLER_H_
#define CONTENT_BROWSER_TRACING_TRACING_SERVICE_CONTROLLER_H_

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"
#include "services/tracing/public/mojom/tracing_service.mojom.h"

namespace content {

// Processes participating in tracing must register themselves with the global
// instance of this object.
class TracingServiceController {
 public:
  // An object held for the duration of any client process's registration with
  // the tracing service.
  class ClientRegistration {
   public:
    ClientRegistration(base::PassKey<TracingServiceController>,
                       base::OnceClosure unregister);
    ~ClientRegistration();

   private:
    base::OnceClosure unregister_;
  };

  TracingServiceController(const TracingServiceController&) = delete;
  ~TracingServiceController();
  TracingServiceController& operator=(const TracingServiceController&) = delete;

  // Gets the global instance of this object. Safe to call from any sequence,
  // but see individual methods for other potential constraints.
  static TracingServiceController& Get();

  // A callback each process host must provide to implement how a TracedProcess
  // interface is bound in the corresponding client process. Callbacks are
  // always called from the UI thread.
  using EnableTracingCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<tracing::mojom::TracedProcess>)>;

  // Registers a new client process with the tracing system. Any time the
  // tracing service is started, |callback| will be invoked to connect the
  // client process to the service.
  //
  // The process will remain registered as long as the returned
  // ClientRegistration object remains alive.
  //
  // Safe to call from any sequence.
  std::unique_ptr<ClientRegistration> RegisterClient(
      base::ProcessId pid,
      EnableTracingCallback callback);

  // Retrieves a remote interface to the tracing service, which is started
  // lazily if needed. Public content API consumers can use
  // |content::GetTracingService()|.
  //
  // Must only be called on the UI thread.
  tracing::mojom::TracingService& GetService();

 private:
  friend class base::NoDestructor<TracingServiceController>;

  TracingServiceController();

  void RegisterClientOnUIThread(base::ProcessId pid,
                                EnableTracingCallback callback);
  void RemoveClient(base::ProcessId pid);

  // NOTE: This state is accessed only from the UI thread.
  mojo::Remote<tracing::mojom::TracingService> service_;
  std::map<base::ProcessId, EnableTracingCallback> clients_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACING_SERVICE_CONTROLLER_H_
