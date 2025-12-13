// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_UI_SHORTCUTS_CONSUMER_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_UI_SHORTCUTS_CONSUMER_H_

@class ShortcutsActionItem;

// Interface for listening to events occurring in ShortcutsMediator.
@protocol ShortcutsConsumer

@optional
// Indicates that the `config` has been updated.
- (void)shortcutsItemConfigDidChange:(ShortcutsActionItem*)config;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_UI_SHORTCUTS_CONSUMER_H_
