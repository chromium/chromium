// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_UI_CONTEXTUAL_PANEL_ENTRYPOINT_VISIBILITY_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_UI_CONTEXTUAL_PANEL_ENTRYPOINT_VISIBILITY_DELEGATE_H_

// A delegate for the contextual entry point visibility.
@protocol ContextualPanelEntrypointVisibilityDelegate

// Show/hide the contextual panel entrypoint.
- (void)setContextualPanelEntrypointHidden:(BOOL)hidden;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_UI_CONTEXTUAL_PANEL_ENTRYPOINT_VISIBILITY_DELEGATE_H_
