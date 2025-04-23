// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_EXPLAIN_WITH_GEMINI_COORDINATOR_EXPLAIN_WITH_GEMINI_DELEGATE_H_
#define IOS_CHROME_BROWSER_EXPLAIN_WITH_GEMINI_COORDINATOR_EXPLAIN_WITH_GEMINI_DELEGATE_H_

@protocol UIMenuBuilder;

// Delegate to add an "Explain with Gemini" UIMenu item.
@protocol ExplainWithGeminiDelegate

// Will be called by `BrowserContainerViewController buildMenuWithBuilder:`
// to customize its edit menu.
- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder;

@end

#endif  // IOS_CHROME_BROWSER_EXPLAIN_WITH_GEMINI_COORDINATOR_EXPLAIN_WITH_GEMINI_DELEGATE_H_
