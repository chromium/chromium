// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_REQUEST_QUEUE_CALLBACK_INSTALLER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_REQUEST_QUEUE_CALLBACK_INSTALLER_H_

#include <memory>

#include "ios/chrome/browser/overlays/model/public/overlay_modality.h"

class OverlayRequestCallbackInstaller;

namespace web {
class WebState;
}

// Helper object that installs callbacks on every OverlayRequest added to the
// OverlayRequestQueue for a given Browser and OverlayModality.
class OverlayRequestQueueCallbackInstaller {
 public:
  // Creates an installer for requests in `web_state`'s queue at `modality`.
  static std::unique_ptr<OverlayRequestQueueCallbackInstaller> Create(
      web::WebState* web_state,
      OverlayModality modality);

  OverlayRequestQueueCallbackInstaller(
      const OverlayRequestQueueCallbackInstaller&) = delete;
  OverlayRequestQueueCallbackInstaller& operator=(
      const OverlayRequestQueueCallbackInstaller&) = delete;
  virtual ~OverlayRequestQueueCallbackInstaller() = default;

  // Adds a callback installer for requests added to the WebState's queue.
  // InstallCallbacks() will be called on every added request callback installer
  // for every OverlayRequest added to the queue.
  virtual void AddRequestCallbackInstaller(
      std::unique_ptr<OverlayRequestCallbackInstaller> installer) = 0;

 protected:
  // OverlayRequestQueueCallbackInstallers must be created using factory method.
  OverlayRequestQueueCallbackInstaller() = default;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_REQUEST_QUEUE_CALLBACK_INSTALLER_H_
