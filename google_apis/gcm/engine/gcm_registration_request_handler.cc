// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/gcm_registration_request_handler.h"

#include "base/metrics/histogram_functions.h"
#include "google_apis/gcm/base/gcm_util.h"

namespace gcm {

namespace {

// Request constants.
const char kSenderKey[] = "sender";

}  // namespace

GCMRegistrationRequestHandler::GCMRegistrationRequestHandler(
    const std::string& senders)
    : senders_(senders) {}

GCMRegistrationRequestHandler::~GCMRegistrationRequestHandler() = default;

void GCMRegistrationRequestHandler::BuildRequestBody(std::string* body) {
  BuildFormEncoding(kSenderKey, senders_, body);
}

void GCMRegistrationRequestHandler::ReportStatusToUMA(
    RegistrationRequest::Status status,
    const std::string& subtype) {
  base::UmaHistogramEnumeration("GCM.RegistrationRequestStatus", status);
}

void GCMRegistrationRequestHandler::ReportNetErrorCodeToUMA(
    int net_error_code) {
  base::UmaHistogramSparse("GCM.RegistrationRequest.NetErrorCode",
                           std::abs(net_error_code));
}

}  // namespace gcm
