
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/ukm_recorder_factory_impl.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/metrics/public/cpp/ukm_recorder_client_interface_registry.h"
#include "services/metrics/ukm_recorder_interface.h"

namespace metrics {

UkmRecorderFactoryImpl::UkmRecorderFactoryImpl(ukm::UkmRecorder* ukm_recorder)
    : ukm_recorder_(ukm_recorder) {}

UkmRecorderFactoryImpl::~UkmRecorderFactoryImpl() = default;

void UkmRecorderFactoryImpl::Create(
    ukm::UkmRecorder* ukm_recorder,
    mojo::PendingReceiver<ukm::mojom::UkmRecorderFactory> receiver) {
  // Binds the lifetime of an UkmRecorderFactory implementation to the lifetime
  // of the |receiver|. When the |receiver| is disconnected (typically by the
  // remote end closing the entangled Remote), the implementation will be
  // deleted.
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<UkmRecorderFactoryImpl>(ukm_recorder),
      std::move(receiver));
}

void UkmRecorderFactoryImpl::CreateUkmRecorder(
    mojo::PendingReceiver<ukm::mojom::UkmRecorderInterface> receiver,
    mojo::PendingRemote<ukm::mojom::UkmRecorderClientInterface> client_remote) {
  metrics::UkmRecorderInterface::Create(ukm_recorder_, std::move(receiver));
  // |client_remote| is null when kUkmReduceAddEntryIPC feature is disabled and
  // thus UkmRecorderClientInterface is not attached.
  if (client_remote.is_valid()) {
    metrics::UkmRecorderClientInterfaceRegistry::AddClientToCurrentRegistry(
        std::move(client_remote));
  }
}

}  // namespace metrics
