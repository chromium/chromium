// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NOTIFICATIONS_MODULE_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NOTIFICATIONS_MODULE_DELEGATE_H_

enum class ContentSuggestionsModuleType;

// Protocol for relaying notification events from Magic Stack modules to Content
// Suggestions.
@protocol NotificationsModuleDelegate

// Enables notifications for the module of the given `type`.
- (void)enableNotifications:(ContentSuggestionsModuleType)type;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NOTIFICATIONS_MODULE_DELEGATE_H_
