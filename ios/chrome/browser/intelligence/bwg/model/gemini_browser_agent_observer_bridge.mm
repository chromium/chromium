// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent_observer_bridge.h"

GeminiBrowserAgentObserverBridge::GeminiBrowserAgentObserverBridge(
    id<GeminiBrowserAgentObserving> owner,
    GeminiBrowserAgent* agent)
    : owner_(owner) {
  scoped_observation_.Observe(agent);
}

GeminiBrowserAgentObserverBridge::~GeminiBrowserAgentObserverBridge() = default;

void GeminiBrowserAgentObserverBridge::OnFloatyInvokedChanged(bool is_invoked) {
  if ([owner_ respondsToSelector:@selector(geminiFloatyInvokedChanged:)]) {
    [owner_ geminiFloatyInvokedChanged:is_invoked];
  }
}

void GeminiBrowserAgentObserverBridge::OnGeminiAvailabilityChanged(
    bool available) {
  if ([owner_ respondsToSelector:@selector(geminiAvailabilityChanged:)]) {
    [owner_ geminiAvailabilityChanged:available];
  }
}
