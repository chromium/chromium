// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/instance_id_get_token_request_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "google_apis/gcm/base/gcm_util.h"

namespace gcm {

namespace {

// Request constants.
const char kAuthorizedEntityKey[] = "sender";
const char kGMSVersionKey[] = "gmsv";
const char kInstanceIDKey[] = "appid";
const char kScopeKey[] = "scope";
const char kExtraScopeKey[] = "X-scope";
const char kTimeToLiveSecondsKey[] = "ttl";

}  // namespace

InstanceIDGetTokenRequestHandler::InstanceIDGetTokenRequestHandler(
    const std::string& instance_id,
    const std::string& authorized_entity,
    const std::string& scope,
    int gcm_version,
    base::TimeDelta time_to_live)
    : instance_id_(instance_id),
      authorized_entity_(authorized_entity),
      scope_(scope),
      gcm_version_(gcm_version),
      time_to_live_(time_to_live) {
  DCHECK(!instance_id.empty());
  DCHECK(!authorized_entity.empty());
  DCHECK(!scope.empty());
}

InstanceIDGetTokenRequestHandler::~InstanceIDGetTokenRequestHandler() = default;

void InstanceIDGetTokenRequestHandler::BuildRequestBody(std::string* body) {
  BuildFormEncoding(kScopeKey, scope_, body);
  BuildFormEncoding(kExtraScopeKey, scope_, body);
  BuildFormEncoding(kGMSVersionKey, base::NumberToString(gcm_version_), body);
  BuildFormEncoding(kInstanceIDKey, instance_id_, body);
  BuildFormEncoding(kAuthorizedEntityKey, authorized_entity_, body);
  if (!time_to_live_.is_zero()) {
    BuildFormEncoding(kTimeToLiveSecondsKey,
                      base::NumberToString(time_to_live_.InSeconds()), body);
  }
}

void InstanceIDGetTokenRequestHandler::ReportStatusToUMA(
    RegistrationRequest::Status status,
    const std::string& subtype) {
  base::UmaHistogramEnumeration("InstanceID.GetToken.RequestStatus", status);

  // For some specific subtypes, also record separate histograms. This makes
  // sense for large users (who might want to look at the status of their
  // requests specifically), or for deep dives into unexplained changes to the
  // top-level "InstanceID.GetToken.RequestStatus" histogram.
  if (subtype == "com.google.chrome.sync.invalidations") {
    base::UmaHistogramEnumeration(
        "InstanceID.GetToken.RequestStatus.SyncInvalidations", status);
  }
}

void InstanceIDGetTokenRequestHandler::ReportNetErrorCodeToUMA(
    int net_error_code) {
  base::UmaHistogramSparse("InstanceID.GetToken.RequestNetErrorCode",
                           std::abs(net_error_code));
}

}  // namespace gcm
