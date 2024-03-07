// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_SHORTCUTS_CONSUMER_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_SHORTCUTS_CONSUMER_SOURCE_H_

// The source of any consumer of Shortcuts MagicStack events.
@protocol ShortcutsConsumerSource

// Consumer for this model.
- (void)addConsumer:(id<ShortcutsConsumer>)consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_SHORTCUTS_CONSUMER_SOURCE_H_
