// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_WITH_SEARCH_WITH_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_WITH_SEARCH_WITH_DELEGATE_H_

@protocol UIMenuBuilder;

// Protocol for handling link to text and presenting related UI.
@protocol SearchWithDelegate

- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_WITH_SEARCH_WITH_DELEGATE_H_
