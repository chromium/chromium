// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_NOTIFIER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_NOTIFIER_BROWSER_AGENT_H_

#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

@protocol BrowserViewVisibilityAudience;
@class BrowserViewVisibilityHandler;
class BrowserViewVisibilityObserver;
enum class BrowserViewVisibilityState;

/// Browser agent that handles browser view visibility updates.
class BrowserViewVisibilityNotifierBrowserAgent
    : public BrowserUserData<BrowserViewVisibilityNotifierBrowserAgent> {
 public:
  /// Not copyable or moveable
  BrowserViewVisibilityNotifierBrowserAgent(
      const BrowserViewVisibilityNotifierBrowserAgent&) = delete;
  BrowserViewVisibilityNotifierBrowserAgent& operator=(
      const BrowserViewVisibilityNotifierBrowserAgent&) = delete;
  ~BrowserViewVisibilityNotifierBrowserAgent() override;

  /// Adds `observer` to the list of observers.
  void AddObserver(BrowserViewVisibilityObserver* observer);

  /// Removes `observer` from the list of observers.
  void RemoveObserver(BrowserViewVisibilityObserver* observer);

  /// Retrieve the `BrowserViewVisibilityAudience` object to be attached to the
  /// browser view.
  id<BrowserViewVisibilityAudience> GetBrowserViewVisibilityAudience();

 private:
  friend class BrowserUserData<BrowserViewVisibilityNotifierBrowserAgent>;

  base::WeakPtr<BrowserViewVisibilityNotifierBrowserAgent> AsWeakPtr();

  explicit BrowserViewVisibilityNotifierBrowserAgent(Browser* browser);

  /// Browser visibility has changed from `previous_state` to `current_state`.
  void BrowserViewVisibilityStateDidChange(
      BrowserViewVisibilityState current_state,
      BrowserViewVisibilityState previous_state);

  /// The object that handles visibility updates.
  BrowserViewVisibilityHandler* visibility_handler_;

  /// List of observers for browser view visibility updates.
  base::ObserverList<BrowserViewVisibilityObserver, true> observers_;

  base::WeakPtrFactory<BrowserViewVisibilityNotifierBrowserAgent>
      weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_NOTIFIER_BROWSER_AGENT_H_
