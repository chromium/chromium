// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "base/types/pass_key.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

class FullscreenBrowserAgentTest;
class FullscreenMediatorPassKeyProvider;
enum class FullscreenModeTransitionTrigger;

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

  // Adds an obscured inset for the given edge. Observers should call this
  // during WillUpdateState().
  void AddObscuredInset(UIRectEdge edge, CGFloat amount);

  // Accessors for the insets.
  UIEdgeInsets insets() const { return insets_; }
  UIEdgeInsets min_insets() const { return min_insets_; }
  UIEdgeInsets max_insets() const { return max_insets_; }

  // Accessors for the progress in entering or exiting fullscreen.
  // 1.0 indicates browser UI is fully visible, 0.0 indicates browser UI is
  // fully hidden (in fullscreen mode).
  CGFloat top_progress() const { return top_progress_; }
  CGFloat bottom_progress() const { return bottom_progress_; }

  // Incrementally changes the fullscreen progress based on a drag or scroll.
  void IncrementalScroll(CGFloat amount, PassKey);

  // Enters or exits fullscreen mode.
  void EnterFullscreen(PassKey,
                       FullscreenModeTransitionTrigger trigger,
                       bool animated);
  void ExitFullscreen(PassKey,
                      FullscreenModeTransitionTrigger trigger,
                      bool animated);

  // Increments the disabled counter. If the counter becomes 1, it exits
  // fullscreen mode.
  void IncrementDisabledCounter(PassKey, bool animated);

  // Decrements the disabled counter.
  void DecrementDisabledCounter(PassKey);

  // Returns the disabled counter.
  size_t disabled_count() const { return disabled_count_; }

  // Invalidates the current inset ranges and recalculates them by notifying
  // observers.
  void InvalidateInsetRange();

  // True while InvalidateInsetRange() is running.
  bool invalidating_inset_range() const { return invalidating_inset_range_; }

 private:
  friend class BrowserUserData<FullscreenBrowserAgent>;

  explicit FullscreenBrowserAgent(Browser* browser);

  // Updates the progress and broadcasts the change to observers.
  void UpdateProgressAndBroadcast(CGFloat top_progress,
                                  CGFloat bottom_progress,
                                  bool animated);

  // Notifies all observers of an updated state.
  void NotifyObserversOfUpdatedState();

  base::ObserverList<FullscreenBrowserAgentObserver, true> observers_;

  // The number of features currently disabling fullscreen.
  size_t disabled_count_ = 0;

  // The insets.
  UIEdgeInsets insets_ = UIEdgeInsetsZero;
  UIEdgeInsets min_insets_ = UIEdgeInsetsZero;
  UIEdgeInsets max_insets_ = UIEdgeInsetsZero;

  // True while InvalidateInsetRange() is running.
  bool invalidating_inset_range_ = false;

  // The progress in entering or exiting fullscreen. 1.0 indicates browser UI is
  // fully visible, 0.0 indicates browser UI is fully hidden (in fullscreen
  // mode).
  CGFloat top_progress_ = 1.0;
  CGFloat bottom_progress_ = 1.0;

  // True if the agent is currently broadcasting WillUpdateObscuredInsetRange.
  // Used to ensure AddObscuredInsetRange() is only called at the correct time.
  bool updating_obscured_insets_ = false;

  // True if the agent is currently broadcasting WillUpdateState. Used to
  // ensure AddObscuredInset() is only called a the correct time.
  bool updating_insets_ = false;

  base::WeakPtrFactory<FullscreenBrowserAgent> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_H_
