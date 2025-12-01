// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_UI_SHORTCUTS_CONSUMER_SOURCE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_UI_SHORTCUTS_CONSUMER_SOURCE_H_

@protocol ShortcutsConsumer;

// The source of any consumer of Shortcuts MagicStack events.
@protocol ShortcutsConsumerSource

// Consumer for this model.
- (void)addConsumer:(id<ShortcutsConsumer>)consumer;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_UI_SHORTCUTS_CONSUMER_SOURCE_H_
