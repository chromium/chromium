// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_PRESENTATION_CONTEXT_UTIL_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_PRESENTATION_CONTEXT_UTIL_H_

class OverlayPresentationContext;

// Returns whether |context|'s UIPresentationCapabilities currently support
// overlay UI implemented with contained UIViewControllers.
bool OverlayPresentationContextSupportsContainedUI(
    OverlayPresentationContext* context);

// Returns whether |context|'s UIPresentationCapabilities currently support
// overlay UI implemented with presented UIViewControllers.
bool OverlayPresentationContextSupportsPresentedUI(
    OverlayPresentationContext* context);

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_PRESENTATION_CONTEXT_UTIL_H_
