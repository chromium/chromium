// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_TAB_CHANGE_RESPONDER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_TAB_CHANGE_RESPONDER_H_

/// Responds to tab changes.
@protocol LensOverlayTabChangeResponder

/// Notifies the responder that a tab change is about to happen in the
/// background.
- (void)prepareForBackgroundTabChange;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_TAB_CHANGE_RESPONDER_H_
