// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_XR_RUNTIME_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_XR_RUNTIME_H_

#include "base/observer_list_types.h"
#include "content/common/content_export.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"

namespace content {
class WebContents;
}

namespace content {
// This interface allows observing the state of the XR service for a particular
// runtime.  In particular, observers may currently know when the browser
// considers a WebContents presenting to an immersive headset.  Implementers of
// this interface will be called on the main browser thread.  Currently this is
// used on Windows to drive overlays.
class CONTENT_EXPORT BrowserXRRuntime {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void SetVRDisplayInfo(
        device::mojom::VRDisplayInfoPtr display_info) {}

    // The parameter |contents| is set when a page starts an immersive WebXR
    // session. There can only be at most one active immersive session for the
    // XRRuntime. Set to null when there is no active immersive session.
    virtual void SetWebXRWebContents(content::WebContents* contents) {}

    virtual void SetFramesThrottled(bool throttled) {}
  };

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_XR_RUNTIME_H_
