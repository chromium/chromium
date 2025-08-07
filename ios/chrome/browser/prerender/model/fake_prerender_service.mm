// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/model/fake_prerender_service.h"

#import "ios/chrome/browser/prerender/model/prerender_tab_helper.h"

FakePrerenderService::FakePrerenderService() = default;

FakePrerenderService::~FakePrerenderService() = default;

void FakePrerenderService::SetPrerenderWebState(web::WebState* web_state) {
  PrerenderTabHelper::CreateForWebState(web_state, this);
}

void FakePrerenderService::SetDelegate(id<PreloadControllerDelegate> delegate) {
}

void FakePrerenderService::StartPrerender(const GURL& url,
                                          const web::Referrer& referrer,
                                          ui::PageTransition transition,
                                          web::WebState* web_state_to_replace,
                                          bool immediately) {
  preload_url_ = url;
}

bool FakePrerenderService::MaybeLoadPrerenderedURL(
    const GURL& url,
    ui::PageTransition transition,
    Browser* browser) {
  preload_url_ = GURL();
  return false;
}

bool FakePrerenderService::IsLoadingPrerender() {
  return false;
}

void FakePrerenderService::CancelAllPrerenders() {
  preload_url_ = GURL();
}

bool FakePrerenderService::HasPrerenderForUrl(const GURL& url) {
  return preload_url_ == url;
}

bool FakePrerenderService::IsWebStatePrerendered(web::WebState* web_state) {
  return PrerenderTabHelper::FromWebState(web_state) != nullptr;
}

void FakePrerenderService::CancelPrerender() {
  CancelAllPrerenders();
}
