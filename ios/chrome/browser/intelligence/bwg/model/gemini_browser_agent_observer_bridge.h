// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_AGENT_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_AGENT_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/scoped_observation.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"

// Objective-C protocol to be implemented by classes that want to observe
// GeminiBrowserAgent events.
@protocol GeminiBrowserAgentObserving <NSObject>
@optional

// Invoked when the Gemini floaty invocation state changes.
- (void)geminiFloatyInvokedChanged:(BOOL)isInvoked;

// Invoked when Gemini availability for the active web state changes.
- (void)geminiAvailabilityChanged:(BOOL)available;

@end

// Bridge class to observe GeminiBrowserAgent in Objective-C.
class GeminiBrowserAgentObserverBridge : public GeminiBrowserAgent::Observer {
 public:
  GeminiBrowserAgentObserverBridge(id<GeminiBrowserAgentObserving> owner,
                                   GeminiBrowserAgent* agent);
  ~GeminiBrowserAgentObserverBridge() override;

  // GeminiBrowserAgent::Observer overrides:
  void OnFloatyInvokedChanged(bool is_invoked) override;
  void OnGeminiAvailabilityChanged(bool available) override;

 private:
  __weak id<GeminiBrowserAgentObserving> owner_ = nil;
  base::ScopedObservation<GeminiBrowserAgent, GeminiBrowserAgent::Observer>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_AGENT_OBSERVER_BRIDGE_H_
