// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/gcm_registration_request_handler.h"

#include "base/metrics/histogram_macros.h"
#include "google_apis/gcm/base/gcm_util.h"

namespace gcm {

namespace {

// Request constants.
const char kSenderKey[] = "sender";

}  // namespace

GCMRegistrationRequestHandler::GCMRegistrationRequestHandler(
    const std::string& senders)
    : senders_(senders) {
}

GCMRegistrationRequestHandler::~GCMRegistrationRequestHandler() {}

void GCMRegistrationRequestHandler::BuildRequestBody(std::string* body){
  BuildFormEncoding(kSenderKey, senders_, body);
}

void GCMRegistrationRequestHandler::ReportUMAs(
    RegistrationRequest::Status status) {
  UMA_HISTOGRAM_ENUMERATION("GCM.RegistrationRequestStatus",
                            status,
                            RegistrationRequest::STATUS_COUNT);
}

}  // namespace gcm
