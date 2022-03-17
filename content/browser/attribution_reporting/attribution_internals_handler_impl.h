// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_HANDLER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_HANDLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "content/browser/attribution_reporting/attribution_internals.mojom.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace content {

class AttributionManagerProvider;
class WebUI;

// Implements the mojo endpoint for the attribution internals WebUI which
// proxies calls to the `AttributionManager` to get information about stored
// attribution data. Also observes the manager in order to push events, e.g.
// reports being sent or dropped, to the internals WebUI. Owned by
// `AttributionInternalsUI`.
class AttributionInternalsHandlerImpl
    : public mojom::AttributionInternalsHandler,
      public AttributionObserver {
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
      AttributionReport::ReportType report_type,
      mojom::AttributionInternalsHandler::GetReportsCallback callback) override;
  void SendEventLevelReports(
      const std::vector<AttributionReport::EventLevelData::Id>& ids,
      mojom::AttributionInternalsHandler::SendEventLevelReportsCallback
          callback) override;
  void SendAggregatableAttributionReports(
      const std::vector<AttributionReport::AggregatableAttributionData::Id>&
          ids,
      mojom::AttributionInternalsHandler::
          SendAggregatableAttributionReportsCallback callback) override;
  void ClearStorage(mojom::AttributionInternalsHandler::ClearStorageCallback
                        callback) override;
  void AddObserver(
      mojo::PendingRemote<mojom::AttributionInternalsObserver> observer,
      mojom::AttributionInternalsHandler::AddObserverCallback callback)
      override;

  void SetAttributionManagerProviderForTesting(
      std::unique_ptr<AttributionManagerProvider> manager_provider);

 private:
  // AttributionObserver:
  void OnSourcesChanged() override;
  void OnReportsChanged(AttributionReport::ReportType report_type) override;
  void OnSourceDeactivated(
      const DeactivatedSource& deactivated_source) override;
  void OnSourceHandled(const StorableSource& source,
                       StorableSource::Result result) override;
  void OnReportSent(const AttributionReport& report,
                    bool is_debug_report,
                    const SendResult& info) override;
  void OnTriggerHandled(const CreateReportResult& result) override;

  void SendReports(const std::vector<AttributionReport::Id> ids,
                   base::OnceClosure callback);

  raw_ptr<WebUI> web_ui_;
  std::unique_ptr<AttributionManagerProvider> manager_provider_;

  mojo::Receiver<mojom::AttributionInternalsHandler> receiver_;

  mojo::RemoteSet<mojom::AttributionInternalsObserver> observers_;

  base::ScopedObservation<AttributionManager, AttributionObserver>
      manager_observation_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_HANDLER_IMPL_H_
