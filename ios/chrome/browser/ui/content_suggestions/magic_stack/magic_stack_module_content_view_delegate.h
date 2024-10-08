// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_CONTENT_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_CONTENT_VIEW_DELEGATE_H_

// Interface for listening to events occurring in MagicStackModuleContainer.
@protocol MagicStackModuleContentViewDelegate

// Sets the subtitle text of the Magic Stack Module Container to `subtitle`.
- (void)setSubtitle:(NSString*)subtitle;

// Updates the visibility of the notifications opt-in button within the Magic
// Stack Module Container based on `showNotificationsOptIn`.
- (void)updateNotificationsOptInVisibility:(BOOL)showNotificationsOptIn;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_CONTENT_VIEW_DELEGATE_H_
