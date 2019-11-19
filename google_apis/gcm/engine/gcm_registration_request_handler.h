// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_GCM_REGISTRATION_REQUEST_HANDLER_H_
#define GOOGLE_APIS_GCM_ENGINE_GCM_REGISTRATION_REQUEST_HANDLER_H_

#include "base/macros.h"
#include "google_apis/gcm/engine/registration_request.h"

namespace gcm {

// Used to obtain the registration ID for applications that want to use GCM.
class GCM_EXPORT GCMRegistrationRequestHandler :
    public RegistrationRequest::CustomRequestHandler {
 public:
  GCMRegistrationRequestHandler(const std::string& senders);
  ~GCMRegistrationRequestHandler() override;

  // RegistrationRequest::RequestHandler overrides:
  void BuildRequestBody(std::string* body) override;
  void ReportUMAs(RegistrationRequest::Status status) override;

 private:
  std::string senders_;

  DISALLOW_COPY_AND_ASSIGN(GCMRegistrationRequestHandler);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_GCM_REGISTRATION_REQUEST_HANDLER_H_
