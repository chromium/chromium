// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_PRESENTATION_CONTEXT_OBSERVER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_PRESENTATION_CONTEXT_OBSERVER_H_

#import <UIKit/UIKit.h>

#include "base/observer_list_types.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presentation_context.h"

// Observer class for the ObserverPresentationContext.
class OverlayPresentationContextObserver : public base::CheckedObserver {
 public:
  OverlayPresentationContextObserver() = default;

  // Called before `presentation_context`'s activation state changes to
  // `activating`.
  virtual void OverlayPresentationContextWillChangePresentationCapabilities(
      OverlayPresentationContext* presentation_context,
      OverlayPresentationContext::UIPresentationCapabilities capabilities) {}

  // Called after `presentation_context`'s activation state changes.
  virtual void OverlayPresentationContextDidChangePresentationCapabilities(
      OverlayPresentationContext* presentation_context) {}

  virtual void OverlayPresentationContextDidEnableUI(
      OverlayPresentationContext* presentation_context) {}

  // Called when `presentation_context` moves to `window`.
  virtual void OverlayPresentationContextDidMoveToWindow(
      OverlayPresentationContext* presentation_context,
      UIWindow* window) {}
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_OVERLAY_PRESENTATION_CONTEXT_OBSERVER_H_
