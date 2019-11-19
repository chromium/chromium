// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_PRERENDER_SERVICE_H_
#define IOS_CHROME_BROWSER_PRERENDER_PRERENDER_SERVICE_H_

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/web/public/navigation/referrer.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@class PreloadController;
@protocol PreloadControllerDelegate;
@protocol SessionWindowRestoring;
namespace ios {
class ChromeBrowserState;
}
namespace web {
class WebState;
}
class WebStateList;

// PrerenderService manages a prerendered WebState.
class PrerenderService : public KeyedService {
 public:
  // TODO(crbug.com/754050): Convert this constructor to take lower-level
  // objects instead of the entire ChromeBrowserState.  This will make unit
  // testing much simpler.
  PrerenderService(ios::ChromeBrowserState* browser_state);
  ~PrerenderService() override;

  // Sets the delegate that will provide information to this service.
  void SetDelegate(id<PreloadControllerDelegate> delegate);

  // Prerenders the given |url| with the given |transition|.  Normally,
  // prerender requests are fulfilled after a short delay, to prevent
  // unnecessary prerenders while the user is typing.  If |immediately| is YES,
  // this method starts prerendering immediately, with no delay.  |immediately|
  // should be set to YES only when there is a very high confidence that the
  // user will navigate to the given |url|.
  //
  // If there is already an existing request for |url|, this method does nothing
  // and does not reset the delay timer.  If there is an existing request for a
  // different URL, this method cancels that request and queues this request
  // instead.
  void StartPrerender(const GURL& url,
                      const web::Referrer& referrer,
                      ui::PageTransition transition,
                      bool immediately);

  // If |url| is prerendered, loads the prerendered web state into
  // |web_state_list| at the active index, replacing the existing active web
  // state and saving the session (via |restorer|). If not, or if it isn't
  // possible to replace the active web state, cancels the active preload.
  // Metrics and snapshots are appropriately updated. Returns true if the active
  // webstate was replaced, false otherwise.
  bool MaybeLoadPrerenderedURL(const GURL& url,
                               ui::PageTransition transition,
                               WebStateList* web_state_list,
                               id<SessionWindowRestoring> restorer);

  // |true| while a prerendered webstate is being inserted into a webStateList.
  bool IsLoadingPrerender() { return loading_prerender_; }

  // Cancels any outstanding prerender requests and destroys any prerendered
  // pages.
  void CancelPrerender();

  // Returns true if there is a prerender for the given |url|.
  bool HasPrerenderForUrl(const GURL& url);

  // Returns true if the given |web_state| is being prerendered.
  bool IsWebStatePrerendered(web::WebState* web_state);

  // KeyedService implementation.
  void Shutdown() override;

 private:
  __strong PreloadController* controller_;

  bool loading_prerender_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderService);
};

#endif  // IOS_CHROME_BROWSER_PRERENDER_PRERENDER_SERVICE_H_
