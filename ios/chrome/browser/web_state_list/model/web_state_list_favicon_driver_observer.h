// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_STATE_LIST_FAVICON_DRIVER_OBSERVER_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_STATE_LIST_FAVICON_DRIVER_OBSERVER_H_

#include "base/scoped_multi_source_observation.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/favicon/ios/web_favicon_driver.h"
#include "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

class Browser;

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
    : public TabsDependencyInstaller,
      public favicon::FaviconDriverObserver {
 public:
  WebStateListFaviconDriverObserver(Browser* browser,
                                    id<WebStateFaviconDriverObserver> observer);

  WebStateListFaviconDriverObserver(const WebStateListFaviconDriverObserver&) =
      delete;
  WebStateListFaviconDriverObserver& operator=(
      const WebStateListFaviconDriverObserver&) = delete;

  ~WebStateListFaviconDriverObserver() override;

  // TabsDependencyInstaller implementation:
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

  // favicon::FaviconDriverObserver implementation.
  void OnFaviconUpdated(favicon::FaviconDriver* driver,
                        NotificationIconType icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

 private:
  // The WebStateFaviconDriverObserver to which the FaviconDriver notification
  // are forwarded. Should not be nil.
  __weak id<WebStateFaviconDriverObserver> favicon_observer_;

  // Observation of the FaviconDriver instances.
  base::ScopedMultiSourceObservation<favicon::WebFaviconDriver,
                                     favicon::FaviconDriverObserver>
      favicon_driver_observations_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_STATE_LIST_FAVICON_DRIVER_OBSERVER_H_
