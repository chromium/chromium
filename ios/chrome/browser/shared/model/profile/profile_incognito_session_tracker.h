// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_INCOGNITO_SESSION_TRACKER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_INCOGNITO_SESSION_TRACKER_H_

#include "base/functional/callback.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "ios/chrome/browser/shared/model/browser/browser_list_observer.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

// Tracks whether a given profile has any open off-the-record tabs open,
// invoking a callback when the state changes. Can also be queried to
// check whether any off-the-record tabs are open at any time.
class ProfileIncognitoSessionTracker final : public BrowserListObserver,
                                             public WebStateListObserver {
 public:
  // Callback invoked when the presence of off-the-record tabs has changed.
  using Callback = base::RepeatingCallback<void(bool)>;

  ProfileIncognitoSessionTracker(BrowserList* list, Callback callback);
  ~ProfileIncognitoSessionTracker() final;

  // Returns whether any of the BrowserList's Browser has incognito tabs open.
  bool has_incognito_tabs() const { return has_incognito_tabs_; }

  // BrowserListObserver:
  void OnBrowserAdded(const BrowserList* list, Browser* browser) final;
  void OnBrowserRemoved(const BrowserList* list, Browser* browser) final;
  void OnBrowserListShutdown(BrowserList* list) final;

  // WebStateListObserver:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) final;
  void BatchOperationEnded(WebStateList* web_state_list) final;

 private:
  // Invoked when a potentially significant change is detected in any of
  // the observed WebStateList.
  void OnWebStateListChanged();

  // Closure invoked when the presence of off-the-record tabs has changed.
  Callback callback_;

  // Manages the observation of the BrowserList.
  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};

  // Manages the observation of all off-the-record WebStateLists.
  base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>
      web_state_list_observations_{this};

  // Whether any of the WebStateList has an off-the-record tab open.
  bool has_incognito_tabs_ = false;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_INCOGNITO_SESSION_TRACKER_H_
