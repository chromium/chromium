// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_GCM_REGISTRATION_REQUEST_HANDLER_H_
#define GOOGLE_APIS_GCM_ENGINE_GCM_REGISTRATION_REQUEST_HANDLER_H_

#include "google_apis/gcm/engine/registration_request.h"

namespace gcm {

// Used to obtain the registration ID for applications that want to use GCM.
class GCM_EXPORT GCMRegistrationRequestHandler
    : public RegistrationRequest::CustomRequestHandler {
 public:
  GCMRegistrationRequestHandler(const std::string& senders);

  GCMRegistrationRequestHandler(const GCMRegistrationRequestHandler&) = delete;
  GCMRegistrationRequestHandler& operator=(
      const GCMRegistrationRequestHandler&) = delete;

  ~GCMRegistrationRequestHandler() override;

  // RegistrationRequest::RequestHandler overrides:
  void BuildRequestBody(std::string* body) override;
  void ReportStatusToUMA(RegistrationRequest::Status status,
                         const std::string& subtype) override;
  void ReportNetErrorCodeToUMA(int net_error_code) override;

 private:
  std::string senders_;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_GCM_REGISTRATION_REQUEST_HANDLER_H_
