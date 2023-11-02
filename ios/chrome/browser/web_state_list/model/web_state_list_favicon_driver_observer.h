// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_STATE_LIST_FAVICON_DRIVER_OBSERVER_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_STATE_LIST_FAVICON_DRIVER_OBSERVER_H_

#include <map>

#include "base/scoped_observation.h"

#include "components/favicon/core/favicon_driver_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

namespace web {
class WebState;
}  // namespace web

@protocol WebStateFaviconDriverObserver
// Forward the call from `driver` OnFaviconUpdated method.
- (void)faviconDriver:(favicon::FaviconDriver*)driver
    didUpdateFaviconForWebState:(web::WebState*)webState;
@end

// Listen to multiple FaviconDrivers for notification that their WebState's
// favicon has changed and forward the notifications to FaviconDriverObserving.
// The class listen to a WebStateList for the creation/replacement/removal
// of WebStates.
class WebStateListFaviconDriverObserver
    : public WebStateListObserver,
      public favicon::FaviconDriverObserver {
 public:
  WebStateListFaviconDriverObserver(WebStateList* web_state_list,
                                    id<WebStateFaviconDriverObserver> observer);

  WebStateListFaviconDriverObserver(const WebStateListFaviconDriverObserver&) =
      delete;
  WebStateListFaviconDriverObserver& operator=(
      const WebStateListFaviconDriverObserver&) = delete;

  ~WebStateListFaviconDriverObserver() override;

  // WebStateListObserver implementation:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // favicon::FaviconDriverObserver implementation.
  void OnFaviconUpdated(favicon::FaviconDriver* driver,
                        NotificationIconType icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

 private:
  // Observes the FaviconDriver for `web_state` and updates the
  // `driver_to_web_state_map_`.
  void AddNewWebState(web::WebState* web_state);

  // Stops observing the FaviconDriver for `web_state` and updates the
  // `driver_to_web_state_map_`.
  void DetachWebState(web::WebState* web_state);

  // The WebStateFaviconDriverObserver to which the FaviconDriver notification
  // are forwarded. Should not be nil.
  __weak id<WebStateFaviconDriverObserver> favicon_observer_;

  // Maps FaviconDriver to the WebState they are attached to. Used
  // to find the WebState that should be passed when forwarding the
  // notification to WebStateFaviconDriverObservers.
  std::map<favicon::FaviconDriver*, web::WebState*> driver_to_web_state_map_;

  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_STATE_LIST_FAVICON_DRIVER_OBSERVER_H_
