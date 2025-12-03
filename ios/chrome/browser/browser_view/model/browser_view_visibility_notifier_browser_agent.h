// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_NOTIFIER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_NOTIFIER_BROWSER_AGENT_H_

#import "base/callback_list.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/browser_view/public/browser_view_visibility_state_changed_callback.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

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

  /// Registers `callback` to be called when the browser visibility changes.
  [[nodiscard]] base::CallbackListSubscription
  RegisterBrowserVisibilityStateChangedCallback(
      const BrowserViewVisibilityStateChangedCallback& callback);

  /// Returns a BrowserViewVisibilityStateChangedCallback that can be used
  /// to notify registered observers of the browser view visibility state
  /// changes.
  BrowserViewVisibilityStateChangedCallback GetNotificationCallback();

 private:
  friend class BrowserUserData<BrowserViewVisibilityNotifierBrowserAgent>;

  base::WeakPtr<BrowserViewVisibilityNotifierBrowserAgent> AsWeakPtr();

  explicit BrowserViewVisibilityNotifierBrowserAgent(Browser* browser);

  /// Notify registered callbacks that the Browser visibility has changed from
  /// `previous_state` to `current_state`.
  void BrowserViewVisibilityStateDidChange(
      BrowserViewVisibilityState current_state,
      BrowserViewVisibilityState previous_state);

  /// List of callbacks notified of the browser visibility changes.
  base::RepeatingCallbackList<
      BrowserViewVisibilityStateChangedCallback::RunType>
      callbacks_;

  base::WeakPtrFactory<BrowserViewVisibilityNotifierBrowserAgent>
      weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_NOTIFIER_BROWSER_AGENT_H_
