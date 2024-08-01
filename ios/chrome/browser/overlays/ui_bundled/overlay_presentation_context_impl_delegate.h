// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_PRESENTATION_CONTEXT_IMPL_DELEGATE_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_PRESENTATION_CONTEXT_IMPL_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_impl.h"

// Delegate protocol used by OverlayPresentationContextImpl to set up the view
// hierarchy to support displaying overlay UI.
@protocol OverlayPresentationContextImplDelegate <NSObject>

// Instructs the delegate to set up the base UIViewController for overlay UI
// that requires `capababilities` and provide it to `context`.
- (void)updatePresentationContext:(OverlayPresentationContextImpl*)context
      forPresentationCapabilities:
          (OverlayPresentationContext::UIPresentationCapabilities)capabilities;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_PRESENTATION_CONTEXT_IMPL_DELEGATE_H_
