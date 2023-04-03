// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_UKM_RECORDER_CLIENT_INTERFACE_REGISTRY_H_
#define SERVICES_METRICS_PUBLIC_CPP_UKM_RECORDER_CLIENT_INTERFACE_REGISTRY_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/metrics/public/cpp/metrics_export.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace metrics {

// Provides a registry to attach MojoUkmRecorder clients to. Clients can be
// added to the registry to be sent updates in the parameters, i.e.,
// mojom::UkmRecorderParameters. All the static methods in this class are
// thread-safe.
class METRICS_EXPORT UkmRecorderClientInterfaceRegistry final {
 public:
  // There can be only one instance of this object at a time which is
  // instantiated by UkmService.
  UkmRecorderClientInterfaceRegistry();
  ~UkmRecorderClientInterfaceRegistry();

  // Adds client to registry and sends current mojom::UkmRecorderParameters
  // back. Thread-safe.
  static void AddClientToCurrentRegistry(
      mojo::PendingRemote<ukm::mojom::UkmRecorderClientInterface>
          pending_remote);

  // Notifies registry about presence of multiple DelegatingUkmRecorder
  // instances in case of tests. Removes all |clients_| and removes the
  // registry. If this is called before a registry_ is created, registry is
  // notified of multiple delegates once it's created. Thread-safe.
  static void NotifyMultipleDelegates();

  // Updates all the clients_ attached to this registry with new parameters.
  void SetRecorderParameters(ukm::mojom::UkmRecorderParametersPtr params);

 private:
  void AddClientOnSequence(
      mojo::PendingRemote<ukm::mojom::UkmRecorderClientInterface>
          pending_remote);

  // Clears |clients_| and deletes the registry.
  void OnMultipleDelegates();

  mojo::RemoteSet<ukm::mojom::UkmRecorderClientInterface> clients_;
  ukm::mojom::UkmRecorderParametersPtr params_;
  base::WeakPtrFactory<UkmRecorderClientInterfaceRegistry> weak_ptr_factory_{
      this};
};
}  // namespace metrics

#endif  // SERVICES_METRICS_PUBLIC_CPP_UKM_RECORDER_CLIENT_INTERFACE_REGISTRY_H_
