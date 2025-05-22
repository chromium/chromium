// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_MUTATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_MUTATOR_H_

// The mutator protocol for the view controller to communicate with the
// mediator.
@protocol PageActionMenuMutator

// Opens Lens overlay.
- (void)openLensOverlay;

// Starts reader mode.
- (void)startReaderMode;

// Starts GLIC overlay.
- (void)startGlicOverlay;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_MUTATOR_H_
