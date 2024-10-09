// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_MEDIATOR_DELEGATE_H_

@class LensOverlayMediator;

/// Delegate for events in LensOverlayMediator.
@protocol LensOverlayMediatorDelegate

/// The lens overlay menu (3-dots) did open.
- (void)lensOverlayMediatorDidOpenOverlayMenu:(LensOverlayMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_MEDIATOR_DELEGATE_H_
