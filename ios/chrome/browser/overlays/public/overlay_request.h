// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_H_

#include <memory>

#include "base/callback.h"
#include "base/supports_user_data.h"

class OverlayResponse;

// Callback for OverlayRequests.  If an overlay requires a completion block to
// be executed after its UI is dismissed, OverlayManager clients can provide a
// callback that uses the OverlayResponse provided to the request.  |response|
// may be null if no response has been provided.
typedef base::OnceCallback<void(OverlayResponse* response)> OverlayCallback;

// Model object used to track overlays requested for OverlayManager.
class OverlayRequest {
 public:
  virtual ~OverlayRequest() = default;

  // Creates an OverlayRequest configured with an OverlayUserData of type
  // ConfigType.  The ConfigType is constructed using the arguments passed to
  // this function.  For example, if a configuration of type StringConfig has
  // a constructor that takes a string, a request configured with a StringConfig
  // can be created using:
  //
  // OverlayRequest::CreateWithConfig<StringConfig>("configuration string");
  template <class ConfigType, typename... Args>
  static std::unique_ptr<OverlayRequest> CreateWithConfig(Args&&... args) {
    std::unique_ptr<OverlayRequest> request = OverlayRequest::Create();
    ConfigType::CreateForUserData(request->data(), std::forward<Args>(args)...);
    return request;
  }

  // Returns the OverlayUserData of type ConfigType stored in the request's
  // user data, or nullptr if it is not found. For example, a configuration of
  // type Config can be retrieved using:
  //
  // request->GetConfig<Config>();
  template <class ConfigType>
  ConfigType* GetConfig() {
    return ConfigType::FromUserData(data());
  }

  // Setter for the response object for this request.
  virtual void set_response(std::unique_ptr<OverlayResponse> response) = 0;
  // The response for this request.  It is constructed with an
  // OverlayResponseInfo containing user interaction information for the overlay
  // UI resulting from this request.
  virtual OverlayResponse* response() const = 0;

  // Setter for the callback.  Provided callbacks are guaranteed to be executed,
  // either upon dismissal of the request's corresponding overlay UI or upon
  // cancellation of the request.
  virtual void set_callback(OverlayCallback callback) = 0;

 protected:
  OverlayRequest() = default;

  // Creates an unconfigured OverlayRequest.
  static std::unique_ptr<OverlayRequest> Create();

  // The container used to hold the user data.
  virtual base::SupportsUserData* data() = 0;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_REQUEST_H_
