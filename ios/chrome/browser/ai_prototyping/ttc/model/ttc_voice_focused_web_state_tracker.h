// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_VOICE_FOCUSED_WEB_STATE_TRACKER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_VOICE_FOCUSED_WEB_STATE_TRACKER_H_

#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "base/observer_list_types.h"
#import "base/scoped_observation.h"
#import "base/sequence_checker.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

class Browser;
class BrowserList;
class WebStateList;

namespace web {
class WebState;
}  // namespace web

// Tracks the active tab's `web::WebState` in the initial active regular
// `Browser` resolved from `BrowserList` and notifies observers whenever it
// changes.
class TtcVoiceFocusedWebStateTracker : public WebStateListObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked whenever the tracked active `WebState` changes. `web_state` is
    // `nullptr` if there is no active tab.
    virtual void OnVoiceFocusedWebStateChanged(web::WebState* web_state) {}
  };

  // Constructs a tracker resolving the active regular `Browser` in
  // `browser_list` and observing its `WebStateList` (if any). `browser_list`
  // may be `nullptr`.
  explicit TtcVoiceFocusedWebStateTracker(BrowserList* browser_list);
  TtcVoiceFocusedWebStateTracker(const TtcVoiceFocusedWebStateTracker&) =
      delete;
  TtcVoiceFocusedWebStateTracker& operator=(
      const TtcVoiceFocusedWebStateTracker&) = delete;
  ~TtcVoiceFocusedWebStateTracker() override;

  // Stops observing the bound `WebStateList` and unbinds the tracked browser.
  void Disconnect();

  // Adds `observer` to receive active `WebState` changes.
  void AddObserver(Observer* observer);

  // Removes `observer` from receiving active `WebState` changes.
  void RemoveObserver(Observer* observer);

  // Returns the active `WebState` of the currently bound regular `Browser`, or
  // `nullptr` if there is no active tab.
  web::WebState* GetActiveWebState() const;

  // `WebStateListObserver` implementation:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;
  void WebStateListDestroyed(WebStateList* web_state_list) override;

 private:
  // Binds to `browser` (or unbinds if `nullptr`) and starts observing its
  // `WebStateList`.
  void BindToBrowser(Browser* browser);

  // Notifies registered observers with `active_web_state`.
  void NotifyVoiceFocusedWebStateChanged(web::WebState* active_web_state);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<Browser> bound_browser_;

  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};

  base::ObserverList<Observer> observers_;
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_VOICE_FOCUSED_WEB_STATE_TRACKER_H_
