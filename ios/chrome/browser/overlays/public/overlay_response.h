// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_RESPONSE_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_RESPONSE_H_

#include <memory>

#include "base/supports_user_data.h"

// Model object used to store information about user interaction with overlay
// UI.
class OverlayResponse {
 public:
  virtual ~OverlayResponse() = default;

  // Creates an OverlayResponse with an OverlayUserData of type InfoType.
  // The InfoType is constructed using the arguments passed to this function.
  // For example, if an info of type IntInfo has a constructor that takes an
  // int, a response with an IntInfo can be created using:
  //
  // OverlayResponse::CreateWithInfo<IntInfo>(0);
  template <class InfoType, typename... Args>
  static std::unique_ptr<OverlayResponse> CreateWithInfo(Args&&... args) {
    std::unique_ptr<OverlayResponse> response = OverlayResponse::Create();
    InfoType::CreateForUserData(response->data(), std::forward<Args>(args)...);
    return response;
  }

  // Returns the OverlayResponseInfo of type InfoType stored in the reponse's
  // user data, or nullptr if it is not found.  For example, an info of type
  // Info can be retrieved using:
  //
  // response->GetInfo<Info>();
  template <class InfoType>
  InfoType* GetInfo() {
    return InfoType::FromUserData(data());
  }

 protected:
  OverlayResponse() = default;

  // Creates an OverlayResponse with no info attached to it.
  static std::unique_ptr<OverlayResponse> Create();

  // The container used to hold the user data.
  virtual base::SupportsUserData* data() = 0;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_RESPONSE_H_
