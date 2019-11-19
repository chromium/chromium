// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/instance_id_get_token_request_handler.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "google_apis/gcm/base/gcm_util.h"
#include "net/url_request/url_request_context_getter.h"

namespace gcm {

namespace {

// Request constants.
const char kAuthorizedEntityKey[] = "sender";
const char kGMSVersionKey[] = "gmsv";
const char kInstanceIDKey[] = "appid";
const char kScopeKey[] = "scope";
const char kExtraScopeKey[] = "X-scope";
// Prefix that needs to be added for each option key.
const char kOptionKeyPrefix[] = "X-";

}  // namespace

InstanceIDGetTokenRequestHandler::InstanceIDGetTokenRequestHandler(
    const std::string& instance_id,
    const std::string& authorized_entity,
    const std::string& scope,
    int gcm_version,
    const std::map<std::string, std::string>& options)
    : instance_id_(instance_id),
      authorized_entity_(authorized_entity),
      scope_(scope),
      gcm_version_(gcm_version),
      options_(options) {
  DCHECK(!instance_id.empty());
  DCHECK(!authorized_entity.empty());
  DCHECK(!scope.empty());
}

InstanceIDGetTokenRequestHandler::~InstanceIDGetTokenRequestHandler() {}

void InstanceIDGetTokenRequestHandler::BuildRequestBody(std::string* body){
  BuildFormEncoding(kScopeKey, scope_, body);
  BuildFormEncoding(kExtraScopeKey, scope_, body);
  for (auto iter = options_.begin(); iter != options_.end(); ++iter)
    BuildFormEncoding(kOptionKeyPrefix + iter->first, iter->second, body);
  BuildFormEncoding(kGMSVersionKey, base::NumberToString(gcm_version_), body);
  BuildFormEncoding(kInstanceIDKey, instance_id_, body);
  BuildFormEncoding(kAuthorizedEntityKey, authorized_entity_, body);
}

void InstanceIDGetTokenRequestHandler::ReportUMAs(
    RegistrationRequest::Status status) {
  UMA_HISTOGRAM_ENUMERATION("InstanceID.GetToken.RequestStatus",
                            status,
                            RegistrationRequest::STATUS_COUNT);
}

}  // namespace gcm
