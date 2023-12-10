// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_REQUEST_SUPPORT_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_REQUEST_SUPPORT_H_

#include <vector>

#include "ios/chrome/browser/overlays/model/public/overlay_request.h"

// Helper object that allows objects to specify support for a subset of
// OverlayRequest types.
class OverlayRequestSupport {
 public:
  // Creates an OverlayResponseSupport that aggregates the request support from
  // the OverlayRequestSupports in `supports`.  Instances created with this
  // constructor will return true from IsRequestSupported() if any of the
  // OverlayRequestSupports in `supports` returns true from IsRequestSupport()
  // for the same request.
  OverlayRequestSupport(
      const std::vector<const OverlayRequestSupport*>& supports);
  virtual ~OverlayRequestSupport();

  // Whether `request` is supported by this instance.  The default
  // implementation returns true is any OverlayRequestSupport in
  // `aggregated_support_` returns true, or false if `aggregated_support_` is
  // empty.
  virtual bool IsRequestSupported(OverlayRequest* request) const;

  // Returns an OverlayRequestSupport that supports all requests.
  static const OverlayRequestSupport* All();

  // Returns an OverlayRequestSupport that does not support any requests.
  static const OverlayRequestSupport* None();

 protected:
  OverlayRequestSupport();

  // The OverlayRequestSupports to aggregate.  Empty for OverlayRequestSupports
  // created with the default constructor.
  const std::vector<const OverlayRequestSupport*> aggregated_support_;
};

// Template used to create OverlayRequestSupports that only support
// OverlayRequests created with a specific ConfigType.
template <class ConfigType>
class SupportsOverlayRequest : public OverlayRequestSupport {
 public:
  SupportsOverlayRequest() = default;
  bool IsRequestSupported(OverlayRequest* request) const override {
    return !!request->GetConfig<ConfigType>();
  }
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_REQUEST_SUPPORT_H_
