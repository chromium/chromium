// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_BROWSER_AGENT_DELEGATE_H_
#define IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_BROWSER_AGENT_DELEGATE_H_

#import <UIKit/UIKit.h>

// A protocol implemented by a delegate of PrerenderBrowserAgent
@protocol PrerenderBrowserAgentDelegate

// Returns the UIView used to contain the WebView for sizing purposes.
- (UIView*)webViewContainer;

@end

#endif  // IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_BROWSER_AGENT_DELEGATE_H_
