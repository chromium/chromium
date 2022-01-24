// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_report.h"

#include <memory>
#include <string>
#include <utility>

#include "base/time/time.h"
#include "base/values.h"
#include "url/gurl.h"

namespace net {

ReportingReport::ReportingReport(
    const absl::optional<base::UnguessableToken>& reporting_source,
    const NetworkIsolationKey& network_isolation_key,
    const GURL& url,
    const std::string& user_agent,
    const std::string& group,
    const std::string& type,
    std::unique_ptr<const base::Value> body,
    int depth,
    base::TimeTicks queued,
    int attempts)
    : reporting_source(reporting_source),
      network_isolation_key(network_isolation_key),
      id(base::UnguessableToken::Create()),
      url(url),
      user_agent(user_agent),
      group(group),
      type(type),
      body(std::move(body)),
      depth(depth),
      queued(queued),
      attempts(attempts) {
  // If |reporting_source| is present, it must not be empty.
  DCHECK(!(reporting_source.has_value() && reporting_source->is_empty()));
}

ReportingReport::ReportingReport() = default;
ReportingReport::ReportingReport(ReportingReport&& other) = default;
ReportingReport& ReportingReport::operator=(ReportingReport&& other) = default;
ReportingReport::~ReportingReport() = default;

ReportingEndpointGroupKey ReportingReport::GetGroupKey() const {
  return ReportingEndpointGroupKey(network_isolation_key, reporting_source,
                                   url::Origin::Create(url), group);
}

bool ReportingReport::IsUploadPending() const {
  return status == Status::PENDING || status == Status::DOOMED ||
         status == Status::SUCCESS;
}

}  // namespace net
