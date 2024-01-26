// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_MODEL_FAKE_PRERENDER_SERVICE_H_
#define IOS_CHROME_BROWSER_PRERENDER_MODEL_FAKE_PRERENDER_SERVICE_H_

#import "ios/chrome/browser/prerender/model/prerender_service.h"

#import "base/memory/raw_ptr.h"

// Fake implementation of PrerenderService. Treats a prerender as in-progress
// after a call to StartPrerender(), but MaybeLoadPrerenderedURL() always
// returns false.
class FakePrerenderService : public PrerenderService {
 public:
  FakePrerenderService();
  ~FakePrerenderService() override;

  // Sets the WebState being prerendered.  Subsequent calls to
  // IsWebStatePrerendered() will return true for `web_state`.
  void set_prerender_web_state(web::WebState* web_state) {
    prerender_web_state_ = web_state;
  }

 private:
  // PrerenderService:
  void SetDelegate(id<PreloadControllerDelegate> delegate) override;
  void StartPrerender(const GURL& url,
                      const web::Referrer& referrer,
                      ui::PageTransition transition,
                      web::WebState* web_state_to_replace,
                      bool immediately) override;
  bool MaybeLoadPrerenderedURL(const GURL& url,
                               ui::PageTransition transition,
                               Browser* browser) override;
  bool IsLoadingPrerender() override;
  void CancelPrerender() override;
  bool HasPrerenderForUrl(const GURL& url) override;
  bool IsWebStatePrerendered(web::WebState* web_state) override;

  raw_ptr<web::WebState> prerender_web_state_ = nullptr;

  // The URL for the in-progress preload.
  GURL preload_url_;
};

#endif  // IOS_CHROME_BROWSER_PRERENDER_MODEL_FAKE_PRERENDER_SERVICE_H_
