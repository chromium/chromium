// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_INTERNALS_HANDLER_IMPL_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_INTERNALS_HANDLER_IMPL_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_observer.h"
#include "content/browser/private_aggregation/private_aggregation_internals.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class WebUI;

// Implements the mojo endpoint for the private aggregation internals WebUI
// which proxies calls to the `AggregationService` to get information about
// stored aggregatable report data. Also observes the manager in order to push
// events, e.g. reports being sent or dropped, to the internals WebUI. Owned by
// `PrivateAggregationInternalsUI`.
//
// NOTE: Today, Private Aggregation is the only API that uses report storage. If
// that changes, this implementation must be updated to filter out reports from
// other APIs.
class CONTENT_EXPORT PrivateAggregationInternalsHandlerImpl
    : public private_aggregation_internals::mojom::Handler,
      public AggregationServiceObserver {
 public:
  PrivateAggregationInternalsHandlerImpl(
      WebUI* web_ui,
      mojo::PendingRemote<private_aggregation_internals::mojom::Observer>,
      mojo::PendingReceiver<private_aggregation_internals::mojom::Handler>);
  PrivateAggregationInternalsHandlerImpl(
      const PrivateAggregationInternalsHandlerImpl&) = delete;
  PrivateAggregationInternalsHandlerImpl(
      PrivateAggregationInternalsHandlerImpl&&) = delete;
  PrivateAggregationInternalsHandlerImpl& operator=(
      const PrivateAggregationInternalsHandlerImpl&) = delete;
  PrivateAggregationInternalsHandlerImpl& operator=(
      PrivateAggregationInternalsHandlerImpl&&) = delete;
  ~PrivateAggregationInternalsHandlerImpl() override;

  // private_aggregation_internals::mojom::Handler:
  void GetReports(
      private_aggregation_internals::mojom::Handler::GetReportsCallback
          callback) override;
  void SendReports(
      const std::vector<AggregationServiceStorage::RequestId>& ids,
      private_aggregation_internals::mojom::Handler::SendReportsCallback
          callback) override;
  void ClearStorage(
      private_aggregation_internals::mojom::Handler::ClearStorageCallback
          callback) override;

 private:
  friend class PrivateAggregationInternalsHandlerImplTest;

  // AggregationServiceObserver:
  void OnRequestStorageModified() override;
  void OnReportHandled(
      const AggregatableReportRequest& request,
      std::optional<AggregationServiceStorage::RequestId> id,
      const std::optional<AggregatableReport>& report,
      base::Time actual_report_time,
      AggregationServiceObserver::ReportStatus result) override;

  void OnObserverDisconnected();

  raw_ptr<WebUI> web_ui_;

  mojo::Remote<private_aggregation_internals::mojom::Observer> observer_;

  mojo::Receiver<private_aggregation_internals::mojom::Handler> handler_;

  // `AggregationService` is bound to the lifetime of the browser context,
  // therefore outlives the observer.
  base::ScopedObservation<AggregationService, AggregationServiceObserver>
      aggregation_service_observer_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_INTERNALS_HANDLER_IMPL_H_
