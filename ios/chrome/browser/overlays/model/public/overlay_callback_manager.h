// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_CALLBACK_MANAGER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_CALLBACK_MANAGER_H_

#include <memory>

#include "ios/chrome/browser/overlays/model/public/overlay_dispatch_callback.h"
#include "ios/chrome/browser/overlays/model/public/overlay_user_data.h"

class OverlayResponse;
// Completion callback for OverlayRequests.  If an overlay requires a completion
// block to be executed after its UI is dismissed, OverlayPresenter clients can
// provide a callback that uses the OverlayResponse provided to the request.
// `response` may be null if no response has been provided.
typedef base::OnceCallback<void(OverlayResponse* response)>
    OverlayCompletionCallback;

// Helper object owned by an OverlayRequest that is used to communicate overlay
// UI interaction information back to the overlay's requester.  Supports
// completion calllbacks, which are executed when the overlay is finished.
class OverlayCallbackManager {
 public:
  OverlayCallbackManager() = default;
  virtual ~OverlayCallbackManager() = default;

  // The completion response object for the request whose callbacks are being
  // managed by this object.  `response` is passed as the argument for
  // completion callbacks when the overlay UI is finished or the request is
  // cancelled.
  virtual void SetCompletionResponse(
      std::unique_ptr<OverlayResponse> response) = 0;
  virtual OverlayResponse* GetCompletionResponse() const = 0;

  // Adds a completion callback.  Provided callbacks are guaranteed to be
  // executed once with the completion response when the overlay UI is finished
  // or the request is cancelled.
  virtual void AddCompletionCallback(OverlayCompletionCallback callback) = 0;

  // Dispatches `response` to all callbacks that have been added for its
  // info type.  Used to send user interaction information back to the overlay's
  // requester for ongoing overlay UI.
  virtual void DispatchResponse(std::unique_ptr<OverlayResponse> response) = 0;

  // Adds `callback` to be executed for dispatched responses.  The provided
  // callbacks are not guaranteed to be called, as there is no guarantee that a
  // supported response will be sent for the overlay.
  virtual void AddDispatchCallback(OverlayDispatchCallback callback) = 0;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_CALLBACK_MANAGER_H_
