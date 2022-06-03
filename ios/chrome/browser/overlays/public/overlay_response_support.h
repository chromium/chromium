// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_RESPONSE_SUPPORT_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_RESPONSE_SUPPORT_H_

#include <vector>

#include "ios/chrome/browser/overlays/public/overlay_response.h"

// Helper object that allows objects to specify support for a subset of
// OverlayResponse types.
class OverlayResponseSupport {
 public:
  // Creates an OverlayResponseSupport that aggregates the support from
  // the OverlayRequestSupports in |supports|.  Instances created with this
  // constructor will return true from IsResponseSupported() if any of the
  // OverlayRequestSupports in |supports| returns true from
  // IsResponseSupported() for the same response.
  OverlayResponseSupport(
      const std::vector<const OverlayResponseSupport*>& supports);
  virtual ~OverlayResponseSupport();

  // Whether |response| is supported by this instance.  The default
  // implementation returns true is any OverlayResponseSupport in
  // |aggregated_support_| returns true, or false if |aggregated_support_| is
  // empty.
  virtual bool IsResponseSupported(OverlayResponse* response) const;

  // Returns an OverlayResponseSupport that supports all responses.
  static const OverlayResponseSupport* All();

  // Returns an OverlayResponseSupport that does not support any responses.
  static const OverlayResponseSupport* None();

 protected:
  OverlayResponseSupport();

  // The OverlayResponseSupports to aggregate.  Empty for
  // OverlayResponseSupports created with the default constructor.
  const std::vector<const OverlayResponseSupport*> aggregated_support_;
};

// Template used to create OverlayResponseSupports that only support
// OverlayResponses created with a specific InfoType.
template <class InfoType>
class SupportsOverlayResponse : public OverlayResponseSupport {
 public:
  SupportsOverlayResponse() = default;
  bool IsResponseSupported(OverlayResponse* response) const override {
    return !!response->GetInfo<InfoType>();
  }
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_OVERLAY_RESPONSE_SUPPORT_H_
