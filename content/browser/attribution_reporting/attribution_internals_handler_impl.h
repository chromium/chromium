// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_HANDLER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_HANDLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "content/browser/attribution_reporting/attribution_internals.mojom.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace content {

class WebUI;

// Implements the mojo endpoint for the attribution internals WebUI which
// proxies calls to the `AttributionManager` to get information about stored
// attribution data. Also observes the manager in order to push events, e.g.
// reports being sent or dropped, to the internals WebUI. Owned by
// `AttributionInternalsUI`.
class AttributionInternalsHandlerImpl
    : public mojom::AttributionInternalsHandler,
      public AttributionManager::Observer {
 public:
  AttributionInternalsHandlerImpl(
      WebUI* web_ui,
      mojo::PendingReceiver<mojom::AttributionInternalsHandler> receiver);
  AttributionInternalsHandlerImpl(
      const AttributionInternalsHandlerImpl& other) = delete;
  AttributionInternalsHandlerImpl& operator=(
      const AttributionInternalsHandlerImpl& other) = delete;
  AttributionInternalsHandlerImpl(AttributionInternalsHandlerImpl&& other) =
      delete;
  AttributionInternalsHandlerImpl& operator=(
      AttributionInternalsHandlerImpl&& other) = delete;
  ~AttributionInternalsHandlerImpl() override;

  // mojom::AttributionInternalsHandler:
  void IsAttributionReportingEnabled(
      mojom::AttributionInternalsHandler::IsAttributionReportingEnabledCallback
          callback) override;
  void GetActiveSources(
      mojom::AttributionInternalsHandler::GetActiveSourcesCallback callback)
      override;
  void GetReports(
      mojom::AttributionInternalsHandler::GetReportsCallback callback) override;
  void SendPendingReports(
      mojom::AttributionInternalsHandler::SendPendingReportsCallback callback)
      override;
  void ClearStorage(mojom::AttributionInternalsHandler::ClearStorageCallback
                        callback) override;
  void AddObserver(
      mojo::PendingRemote<mojom::AttributionInternalsObserver> observer,
      mojom::AttributionInternalsHandler::AddObserverCallback callback)
      override;

  void SetAttributionManagerProviderForTesting(
      std::unique_ptr<AttributionManager::Provider> manager_provider);

 private:
  // AttributionManager::Observer:
  void OnSourcesChanged() override;
  void OnReportsChanged() override;
  void OnSourceDeactivated(
      const AttributionStorage::DeactivatedSource& deactivated_source) override;
  void OnReportSent(const SentReport& info) override;
  void OnReportDropped(
      const AttributionStorage::CreateReportResult& result) override;

  raw_ptr<WebUI> web_ui_;
  std::unique_ptr<AttributionManager::Provider> manager_provider_;

  mojo::Receiver<mojom::AttributionInternalsHandler> receiver_;

  mojo::RemoteSet<mojom::AttributionInternalsObserver> observers_;

  base::ScopedObservation<AttributionManager, AttributionManager::Observer>
      manager_observation_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_HANDLER_IMPL_H_
