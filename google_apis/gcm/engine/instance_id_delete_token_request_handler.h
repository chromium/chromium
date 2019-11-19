// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_INSTANCE_ID_DELETE_TOKEN_REQUEST_HANDLER_H_
#define GOOGLE_APIS_GCM_ENGINE_INSTANCE_ID_DELETE_TOKEN_REQUEST_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "google_apis/gcm/engine/unregistration_request.h"

namespace gcm {

// Provides custom logic to handle DeleteToken request for InstanceID.
class GCM_EXPORT InstanceIDDeleteTokenRequestHandler :
  public UnregistrationRequest::CustomRequestHandler {
 public:
  InstanceIDDeleteTokenRequestHandler(
      const std::string& instance_id,
      const std::string& authorized_entity,
      const std::string& scope,
      int gcm_version);
  ~InstanceIDDeleteTokenRequestHandler() override;

   // UnregistrationRequest overrides:
  void BuildRequestBody(std::string* body) override;
  UnregistrationRequest::Status ParseResponse(
      const std::string& response) override;
  void ReportUMAs(UnregistrationRequest::Status status) override;

 private:
  std::string instance_id_;
  std::string authorized_entity_;
  std::string scope_;
  int gcm_version_;

  DISALLOW_COPY_AND_ASSIGN(InstanceIDDeleteTokenRequestHandler);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_INSTANCE_ID_DELETE_TOKEN_REQUEST_HANDLER_H_
