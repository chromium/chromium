// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_BROWSER_AGENT_OBSERVING_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_BROWSER_AGENT_OBSERVING_H_

class OmniboxPositionBrowserAgent;

// Objective-C protocol for observing OmniboxPositionBrowserAgent.
@protocol OmniboxPositionBrowserAgentObserving <NSObject>

- (void)omniboxPositionBrowserAgent:(OmniboxPositionBrowserAgent*)browser_agent
       isCurrentLayoutBottomOmnibox:(BOOL)isCurrentLayoutBottomOmnibox;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_BROWSER_AGENT_OBSERVING_H_
