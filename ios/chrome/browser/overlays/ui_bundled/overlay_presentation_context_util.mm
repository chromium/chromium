// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_util.h"

#import "ios/chrome/browser/overlays/model/public/overlay_presentation_context.h"

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
