// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_INSTANCE_ID_GET_TOKEN_REQUEST_HANDLER_H_
#define GOOGLE_APIS_GCM_ENGINE_INSTANCE_ID_GET_TOKEN_REQUEST_HANDLER_H_

#include <string>

#include "base/time/time.h"
#include "google_apis/gcm/engine/registration_request.h"

namespace gcm {

// Used to obtain a token based on Instance ID.
class GCM_EXPORT InstanceIDGetTokenRequestHandler
    : public RegistrationRequest::CustomRequestHandler {
 public:
  InstanceIDGetTokenRequestHandler(const std::string& instance_id,
                                   const std::string& authorized_entity,
                                   const std::string& scope,
                                   int gcm_version,
                                   base::TimeDelta time_to_live);

  InstanceIDGetTokenRequestHandler(const InstanceIDGetTokenRequestHandler&) =
      delete;
  InstanceIDGetTokenRequestHandler& operator=(
      const InstanceIDGetTokenRequestHandler&) = delete;

  ~InstanceIDGetTokenRequestHandler() override;

  // RegistrationRequest overrides:
  void BuildRequestBody(std::string* body) override;
  void ReportStatusToUMA(RegistrationRequest::Status status,
                         const std::string& subtype) override;
  void ReportNetErrorCodeToUMA(int net_error_code) override;

 private:
  std::string instance_id_;
  std::string authorized_entity_;
  std::string scope_;
  int gcm_version_;
  base::TimeDelta time_to_live_;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_INSTANCE_ID_GET_TOKEN_REQUEST_HANDLER_H_
