// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/reporting_api_report_mojom_traits.h"

namespace mojo {

// static
network::mojom::ReportingApiReportStatus EnumTraits<
    network::mojom::ReportingApiReportStatus,
    net::ReportingReport::Status>::ToMojom(net::ReportingReport::Status input) {
  switch (input) {
    case net::ReportingReport::Status::QUEUED:
      return network::mojom::ReportingApiReportStatus::kQueued;
    case net::ReportingReport::Status::PENDING:
      return network::mojom::ReportingApiReportStatus::kPending;
    case net::ReportingReport::Status::DOOMED:
      return network::mojom::ReportingApiReportStatus::kDoomed;
    case net::ReportingReport::Status::SUCCESS:
      return network::mojom::ReportingApiReportStatus::kSuccess;
  }
  NOTREACHED_IN_MIGRATION();
  return network::mojom::ReportingApiReportStatus::kQueued;
}

// static
bool EnumTraits<network::mojom::ReportingApiReportStatus,
                net::ReportingReport::Status>::
    FromMojom(network::mojom::ReportingApiReportStatus input,
              net::ReportingReport::Status* output) {
  switch (input) {
    case network::mojom::ReportingApiReportStatus::kQueued:
      *output = net::ReportingReport::Status::QUEUED;
      return true;
    case network::mojom::ReportingApiReportStatus::kPending:
      *output = net::ReportingReport::Status::PENDING;
      return true;
    case network::mojom::ReportingApiReportStatus::kDoomed:
      *output = net::ReportingReport::Status::DOOMED;
      return true;
    case network::mojom::ReportingApiReportStatus::kSuccess:
      *output = net::ReportingReport::Status::SUCCESS;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
bool StructTraits<
    network::mojom::ReportingApiReportDataView,
    net::ReportingReport>::Read(network::mojom::ReportingApiReportDataView data,
                                net::ReportingReport* out) {
  if (!data.ReadId(&out->id))
    return false;
  if (!data.ReadUrl(&out->url))
    return false;
  if (!data.ReadGroup(&out->group))
    return false;
  if (!data.ReadType(&out->type))
    return false;
  if (!data.ReadTimestamp(&out->queued))
    return false;
  out->depth = data.depth();
  out->attempts = data.attempts();
  if (!data.ReadStatus(&out->status))
    return false;

  return data.ReadBody(&out->body);
}

}  // namespace mojo
