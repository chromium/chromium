// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_INTERNALS_HANDLER_IMPL_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_INTERNALS_HANDLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_internals.mojom.h"
#include "content/browser/aggregation_service/aggregation_service_observer.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class WebUI;

// Implements the mojo endpoint for the aggregation service internals WebUI
// which proxies calls to the `AggregationService` to get information about
// stored aggregatable report data. Also observes the manager in order to push
// events, e.g. reports being sent or dropped, to the internals WebUI. Owned by
// `AggregationServiceInternalsUI`.
class CONTENT_EXPORT AggregationServiceInternalsHandlerImpl
    : public aggregation_service_internals::mojom::Handler,
      public AggregationServiceObserver {
 public:
  AggregationServiceInternalsHandlerImpl(
      WebUI* web_ui,
      mojo::PendingRemote<aggregation_service_internals::mojom::Observer>,
      mojo::PendingReceiver<aggregation_service_internals::mojom::Handler>);
  AggregationServiceInternalsHandlerImpl(
      const AggregationServiceInternalsHandlerImpl&) = delete;
  AggregationServiceInternalsHandlerImpl(
      AggregationServiceInternalsHandlerImpl&&) = delete;
  AggregationServiceInternalsHandlerImpl& operator=(
      const AggregationServiceInternalsHandlerImpl&) = delete;
  AggregationServiceInternalsHandlerImpl& operator=(
      AggregationServiceInternalsHandlerImpl&&) = delete;
  ~AggregationServiceInternalsHandlerImpl() override;

  // aggregation_service_internals::mojom::Handler:
  void GetReports(
      aggregation_service_internals::mojom::Handler::GetReportsCallback
          callback) override;
  void SendReports(
      const std::vector<AggregationServiceStorage::RequestId>& ids,
      aggregation_service_internals::mojom::Handler::SendReportsCallback
          callback) override;
  void ClearStorage(
      aggregation_service_internals::mojom::Handler::ClearStorageCallback
          callback) override;

 private:
  friend class AggregationServiceInternalsHandlerImplTest;

  // AggregationServiceObserver:
  void OnRequestStorageModified() override;
  void OnReportHandled(
      const AggregatableReportRequest& request,
      absl::optional<AggregationServiceStorage::RequestId> id,
      const absl::optional<AggregatableReport>& report,
      base::Time actual_report_time,
      AggregationServiceObserver::ReportStatus result) override;

  void OnObserverDisconnected();

  raw_ptr<WebUI> web_ui_;

  mojo::Remote<aggregation_service_internals::mojom::Observer> observer_;

  mojo::Receiver<aggregation_service_internals::mojom::Handler> handler_;

  // `AggregationService` is bound to the lifetime of the browser context,
  // therefore outlives the observer.
  base::ScopedObservation<AggregationService, AggregationServiceObserver>
      aggregation_service_observer_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_INTERNALS_HANDLER_IMPL_H_
