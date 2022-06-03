// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_presentation_context_util.h"

#import "ios/chrome/browser/overlays/public/overlay_presentation_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool OverlayPresentationContextSupportsContainedUI(
    OverlayPresentationContext* context) {
  if (!context)
    return false;
  return context->GetPresentationCapabilities() &
         OverlayPresentationContext::UIPresentationCapabilities::kContained;
}

bool OverlayPresentationContextSupportsPresentedUI(
    OverlayPresentationContext* context) {
  if (!context)
    return false;
  return context->GetPresentationCapabilities() &
         OverlayPresentationContext::UIPresentationCapabilities::kPresented;
}
