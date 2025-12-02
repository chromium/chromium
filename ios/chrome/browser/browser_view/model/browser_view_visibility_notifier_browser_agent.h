// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_NOTIFIER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_NOTIFIER_BROWSER_AGENT_H_

#import "base/callback_list.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

@protocol BrowserViewVisibilityAudience;
@class BrowserViewVisibilityHandler;
enum class BrowserViewVisibilityState;

/// Browser agent that handles browser view visibility updates.
class BrowserViewVisibilityNotifierBrowserAgent
    : public BrowserUserData<BrowserViewVisibilityNotifierBrowserAgent> {
 public:
  /// List of callbacks invoked when the browser visibility changes.
  using VisibilityChangedCallbackList = base::RepeatingCallbackList<void(
      BrowserViewVisibilityState current_state,
      BrowserViewVisibilityState previous_state)>;

  // Callback invoked when the browser visibility changes.
  using VisibilityChangedCallback = VisibilityChangedCallbackList::CallbackType;

  /// Not copyable or moveable
  BrowserViewVisibilityNotifierBrowserAgent(
      const BrowserViewVisibilityNotifierBrowserAgent&) = delete;
  BrowserViewVisibilityNotifierBrowserAgent& operator=(
      const BrowserViewVisibilityNotifierBrowserAgent&) = delete;
  ~BrowserViewVisibilityNotifierBrowserAgent() override;

  /// Registers `callback` to be called when the browser visibility changes.
  [[nodiscard]] base::CallbackListSubscription
  RegisterBrowserVisibilityStateChangedCallback(
      const VisibilityChangedCallback& callback);

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

  /// List of callbacks notified of the browser visibility changes.
  VisibilityChangedCallbackList callbacks_;

  base::WeakPtrFactory<BrowserViewVisibilityNotifierBrowserAgent>
      weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_NOTIFIER_BROWSER_AGENT_H_
