// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_SERVICE_H_
#define IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "ios/web/public/navigation/referrer.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@class PreloadController;
@protocol PreloadControllerDelegate;

namespace web {
class WebState;
}
class WebStateList;
class Browser;

// PrerenderService manages a prerendered WebState.
class PrerenderService : public KeyedService {
 public:
  // Sets the delegate that will provide information to this service.
  virtual void SetDelegate(id<PreloadControllerDelegate> delegate) = 0;

  // Prerenders the given `url` with the given `transition`.  Normally,
  // prerender requests are fulfilled after a short delay, to prevent
  // unnecessary prerenders while the user is typing.  If `immediately` is YES,
  // this method starts prerendering immediately, with no delay.
  // `web_state_to_replace` is provided so that the new prerendered web state
  // can have the same session data.  `immediately` should be set to YES only
  // when there is a very high confidence that the user will navigate to the
  // given `url`.
  // TODO(crbug.com/40726702): passing `web_state_to_replace` is a workaround
  // for not having prerender service per browser, remove it once
  // prerenderService is a browser agent.
  //
  // If there is already an existing request for `url`, this method does nothing
  // and does not reset the delay timer.  If there is an existing request for a
  // different URL, this method cancels that request and queues this request
  // instead.
  virtual void StartPrerender(const GURL& url,
                              const web::Referrer& referrer,
                              ui::PageTransition transition,
                              web::WebState* web_state_to_replace,
                              bool immediately) = 0;

  // If `url` is prerendered, loads the prerendered web state into
  // `browser`'s WebStateList at the active index, replacing the existing active
  // WebState and saving the session. If not, or if it isn't possible to replace
  // the active web state, cancels the active preload. Metrics and snapshots are
  // appropriately updated. Returns true if the active webstate was replaced,
  // false otherwise.
  virtual bool MaybeLoadPrerenderedURL(const GURL& url,
                                       ui::PageTransition transition,
                                       Browser* browser) = 0;

  // `true` while a prerendered webstate is being inserted into a webStateList.
  virtual bool IsLoadingPrerender() = 0;

  // Cancels any outstanding prerender requests and destroys any prerendered
  // pages.
  virtual void CancelPrerender() = 0;

  // Returns true if there is a prerender for the given `url`.
  virtual bool HasPrerenderForUrl(const GURL& url) = 0;

  // Returns true if the given `web_state` is being prerendered.
  virtual bool IsWebStatePrerendered(web::WebState* web_state) = 0;
};

#endif  // IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_SERVICE_H_
