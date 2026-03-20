// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_OBSERVER_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_OBSERVER_H_

#include "base/observer_list_types.h"

class FullscreenBrowserAgent;

// Observer interface for the FullscreenBrowserAgent.
class FullscreenBrowserAgentObserver : public base::CheckedObserver {
 public:
  // Called before the fullscreen state updates.
  virtual void WillUpdateState(FullscreenBrowserAgent* agent) {}

  // Called after the fullscreen state updates.
  virtual void DidUpdateState(FullscreenBrowserAgent* agent) {}

  // Called before the obscured inset range updates.
  virtual void WillUpdateObscuredInsetRange(FullscreenBrowserAgent* agent) {}
};

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_OBSERVER_H_
