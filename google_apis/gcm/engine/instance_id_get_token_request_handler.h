// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_INSTANCE_ID_GET_TOKEN_REQUEST_HANDLER_H_
#define GOOGLE_APIS_GCM_ENGINE_INSTANCE_ID_GET_TOKEN_REQUEST_HANDLER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "google_apis/gcm/engine/registration_request.h"

namespace gcm {

// Used to obtain a token based on Instance ID.
class GCM_EXPORT InstanceIDGetTokenRequestHandler :
    public RegistrationRequest::CustomRequestHandler {
 public:
  InstanceIDGetTokenRequestHandler(
      const std::string& instance_id,
      const std::string& authorized_entity,
      const std::string& scope,
      int gcm_version,
      const std::map<std::string, std::string>& options);
  ~InstanceIDGetTokenRequestHandler() override;

   // RegistrationRequest overrides:
  void BuildRequestBody(std::string* body) override;
  void ReportUMAs(RegistrationRequest::Status status) override;

 private:
  std::string instance_id_;
  std::string authorized_entity_;
  std::string scope_;
  int gcm_version_;
  std::map<std::string, std::string> options_;

  DISALLOW_COPY_AND_ASSIGN(InstanceIDGetTokenRequestHandler);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_INSTANCE_ID_GET_TOKEN_REQUEST_HANDLER_H_
