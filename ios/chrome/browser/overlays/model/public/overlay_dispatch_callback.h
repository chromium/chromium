// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_DISPATCH_CALLBACK_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_DISPATCH_CALLBACK_H_

#include "base/functional/callback.h"
#import "base/memory/raw_ptr.h"

class OverlayResponseSupport;
class OverlayResponse;

// Callback for OverlayResponses dispatched for user interaction events
// occurring in an ongoing overlay.
class OverlayDispatchCallback {
 public:
  // Constructor for a dispatch callback that executes `callback` with
  // OverlayResponses that are supported by `support`.  `callback` and `support`
  // must be non-null.
  OverlayDispatchCallback(
      base::RepeatingCallback<void(OverlayResponse* response)> callback,
      const OverlayResponseSupport* support);
  OverlayDispatchCallback(OverlayDispatchCallback&& other);
  ~OverlayDispatchCallback();

  // Runs `callback_` with `response` iff the response is supported by
  // `request_support_`.
  void Run(OverlayResponse* response);

 private:
  // The callback to be executed.
  base::RepeatingCallback<void(OverlayResponse* response)> callback_;
  // The OverlayResponseSupport determining which dispatch responses can be
  // handled by the callback.
  raw_ptr<const OverlayResponseSupport> response_support_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_DISPATCH_CALLBACK_H_
