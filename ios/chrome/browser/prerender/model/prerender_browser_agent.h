// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_BROWSER_AGENT_H_

#include "base/time/time.h"
#include "ios/chrome/browser/prerender/model/prerender_tab_helper_delegate.h"
#include "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#include "ios/web/public/navigation/referrer.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@class PreloadController;
@protocol PreloadControllerDelegate;

// A BrowserAgent responsible for managing the pre-rendering of web pages.
class PrerenderBrowserAgent final
    : public BrowserUserData<PrerenderBrowserAgent>,
      public PrerenderTabHelperDelegate {
 public:
  // Policy for starting pre-rendering.
  enum PrerenderPolicy {
    kNoDelay,
    kDefaultDelay,
  };

  ~PrerenderBrowserAgent() final;

  // Sets the delegate that will provide information to this agent.
  void SetDelegate(id<PreloadControllerDelegate> delegate);

  // Prerenders the given `url` with the given `transition` after `delay`.
  //
  // If there is already an existing request for `url`, this method does
  // nothing and does not reset the delay. If there is an existing request
  // for a different URL, it is cancelled before the new request is queued.
  //
  // Unless `policy` is `kNoDelay` the pre-rendering is only started after
  // a short delay, to prevent unnecessary pre-rendering while the user is
  // typing.
  void StartPrerender(const GURL& url,
                      const web::Referrer& referrer,
                      ui::PageTransition transition,
                      PrerenderPolicy policy);

  // Promotes the pre-rendered tab to a real tab, replacing the Browser's
  // active WebState if it is used to pre-render `url`. Otherwise cancels
  // the pre-rendering.
  //
  // Returns whether the active WebState was replaced or not.
  bool ValidatePrerender(const GURL& url, ui::PageTransition transition);

  // Returns whether a pre-rendered WebState is being inserted into the
  // Browser by this agent.
  bool IsInsertingPrerender() const;

  // PrerenderTabHelperDelegate implementation.
  void CancelPrerender() final;

 private:
  friend class BrowserUserData<PrerenderBrowserAgent>;
  PrerenderBrowserAgent(Browser* browser);

  __strong PreloadController* controller_;
  bool loading_prerender_ = false;
};

#endif  // IOS_CHROME_BROWSER_PRERENDER_MODEL_PRERENDER_BROWSER_AGENT_H_
