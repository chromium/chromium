// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_MUTATOR_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_MUTATOR_H_

// Protocol for handling toolbar button actions.
@protocol ToolbarMutator

// Navigates back.
- (void)goBack;

// Navigates forward.
- (void)goForward;

// Reloads the current page.
- (void)reload;

// Stops loading the current page.
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_MUTATOR_H_
