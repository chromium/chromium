// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_GCM_UNREGISTRATION_REQUEST_HANDLER_H_
#define GOOGLE_APIS_GCM_ENGINE_GCM_UNREGISTRATION_REQUEST_HANDLER_H_

#include "google_apis/gcm/engine/unregistration_request.h"

namespace gcm {

// Used to revoke the registration ID when unregister is called or the
// application has been uninstalled.
class GCM_EXPORT GCMUnregistrationRequestHandler :
    public UnregistrationRequest::CustomRequestHandler {
 public:
  explicit GCMUnregistrationRequestHandler(const std::string& app_id);

  GCMUnregistrationRequestHandler(const GCMUnregistrationRequestHandler&) =
      delete;
  GCMUnregistrationRequestHandler& operator=(
      const GCMUnregistrationRequestHandler&) = delete;

  ~GCMUnregistrationRequestHandler() override;

  // UnregistrationRequest::CustomRequestHandler overrides:
  void BuildRequestBody(std::string* body) override;
  UnregistrationRequest::Status ParseResponse(
      const std::string& response) override;

 private:
  std::string app_id_;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_GCM_UNREGISTRATION_REQUEST_HANDLER_H_
