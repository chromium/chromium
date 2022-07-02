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

class WebUI;

// Implements the mojo endpoint for the attribution internals WebUI which
// proxies calls to the `AttributionManager` to get information about stored
// attribution data. Also observes the manager in order to push events, e.g.
// reports being sent or dropped, to the internals WebUI. Owned by
// `AttributionInternalsUI`.
class AttributionInternalsHandlerImpl
    : public attribution_internals::mojom::Handler,
      public AttributionObserver {
 public:
  AttributionInternalsHandlerImpl(
      WebUI* web_ui,
      mojo::PendingReceiver<attribution_internals::mojom::Handler> receiver);
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
      attribution_internals::mojom::Handler::
          IsAttributionReportingEnabledCallback callback) override;
  void GetActiveSources(
      attribution_internals::mojom::Handler::GetActiveSourcesCallback callback)
      override;
  void GetReports(AttributionReport::ReportType report_type,
                  attribution_internals::mojom::Handler::GetReportsCallback
                      callback) override;
  void SendReports(const std::vector<AttributionReport::Id>& ids,
                   attribution_internals::mojom::Handler::SendReportsCallback
                       callback) override;
  void ClearStorage(attribution_internals::mojom::Handler::ClearStorageCallback
                        callback) override;
  void AddObserver(
      mojo::PendingRemote<attribution_internals::mojom::Observer> observer,
      attribution_internals::mojom::Handler::AddObserverCallback callback)
      override;

 private:
  // AttributionObserver:
  void OnSourcesChanged() override;
  void OnReportsChanged(AttributionReport::ReportType report_type) override;
  void OnSourceDeactivated(const StoredSource& deactivated_source) override;
  void OnSourceHandled(const StorableSource& source,
                       StorableSource::Result result) override;
  void OnReportSent(const AttributionReport& report,
                    bool is_debug_report,
                    const SendResult& info) override;
  void OnTriggerHandled(const AttributionTrigger& trigger,
                        const CreateReportResult& result) override;

  raw_ptr<WebUI> web_ui_;

  mojo::Receiver<attribution_internals::mojom::Handler> receiver_;

  mojo::RemoteSet<attribution_internals::mojom::Observer> observers_;

  base::ScopedObservation<AttributionManager, AttributionObserver>
      manager_observation_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_HANDLER_IMPL_H_
