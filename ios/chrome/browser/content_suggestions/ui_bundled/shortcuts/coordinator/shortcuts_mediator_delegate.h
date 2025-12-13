// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_COORDINATOR_SHORTCUTS_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_COORDINATOR_SHORTCUTS_MEDIATOR_DELEGATE_H_

enum class ContentSuggestionsModuleType;

// Delegate used to communicate events back to the owner of
// ShortcutsMediator.
@protocol ShortcutsMediatorDelegate

// Logs a user Magic Stack engagement for module `type`.
- (void)logMagicStackEngagementForType:(ContentSuggestionsModuleType)type;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHORTCUTS_COORDINATOR_SHORTCUTS_MEDIATOR_DELEGATE_H_
