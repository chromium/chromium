// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_H_

#import <UIKit/UIKit.h>

#import "base/observer_list.h"
#import "base/types/pass_key.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

class FullscreenBrowserAgentTest;
class FullscreenMediatorPassKeyProvider;

// A class that holds the fullscreen state for a browser.
class FullscreenBrowserAgent : public BrowserUserData<FullscreenBrowserAgent> {
 public:
  // PassKey allows access to methods that mutate the state / progress.
  using PassKey = base::PassKey<FullscreenBrowserAgentTest,
                                FullscreenMediatorPassKeyProvider>;

  ~FullscreenBrowserAgent() override;

  FullscreenBrowserAgent(const FullscreenBrowserAgent&) = delete;
  FullscreenBrowserAgent& operator=(const FullscreenBrowserAgent&) = delete;

  // Adds `observer` to the list of observers.
  void AddObserver(FullscreenBrowserAgentObserver* observer);

  // Removes `observer` from the list of observers.
  void RemoveObserver(FullscreenBrowserAgentObserver* observer);

  // Adds an obscured inset range for the given edge. Observers should call this
  // during WillUpdateObscuredInsetRange().
  void AddObscuredInsetRange(UIRectEdge edge, CGFloat min, CGFloat max);

  // Accessors for the min and max insets.
  UIEdgeInsets min_insets() const { return min_insets_; }
  UIEdgeInsets max_insets() const { return max_insets_; }

  // Instantly exits fullscreen, notifying observers of the update.
  // Generally used to reset the UI from system events like backgrounding.
  void ForceExitFullscreenWithoutAnimation(PassKey);

  // Invalidates the current inset ranges and recalculates them by notifying
  // observers.
  void InvalidateInsetRange(PassKey);

 private:
  friend class BrowserUserData<FullscreenBrowserAgent>;

  explicit FullscreenBrowserAgent(Browser* browser);

  base::ObserverList<FullscreenBrowserAgentObserver, true> observers_;

  // The min and max insets.
  UIEdgeInsets min_insets_ = UIEdgeInsetsZero;
  UIEdgeInsets max_insets_ = UIEdgeInsetsZero;

  // True if the agent is currently broadcasting WillUpdateObscuredInsetRange.
  // Used to ensure AddObscuredInsetRange() is only called at the correct time.
  bool updating_obscured_insets_ = false;
};

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_H_
