// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/favicon_web_state_dispatcher_impl.h"

#include "components/favicon/ios/web_favicon_driver.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Default delay to download the favicon when the WebState is handed back.
const int64_t kDefaultDelayFaviconSecond = 10;
}

namespace reading_list {

FaviconWebStateDispatcherImpl::FaviconWebStateDispatcherImpl(
    web::BrowserState* browser_state,
    int64_t keep_alive_second)
    : FaviconWebStateDispatcher(),
      browser_state_(browser_state),
      keep_alive_second_(keep_alive_second),
      weak_ptr_factory_(this) {
  if (keep_alive_second_ < 0)
    keep_alive_second_ = kDefaultDelayFaviconSecond;
}

FaviconWebStateDispatcherImpl::~FaviconWebStateDispatcherImpl() {}

std::unique_ptr<web::WebState>
FaviconWebStateDispatcherImpl::RequestWebState() {
  const web::WebState::CreateParams web_state_create_params(browser_state_);
  std::unique_ptr<web::WebState> web_state =
      web::WebState::Create(web_state_create_params);

  ios::ChromeBrowserState* original_browser_state =
      ios::ChromeBrowserState::FromBrowserState(browser_state_);

  favicon::WebFaviconDriver::CreateForWebState(
      web_state.get(),
      ios::FaviconServiceFactory::GetForBrowserState(
          original_browser_state, ServiceAccessType::EXPLICIT_ACCESS));

  return web_state;
}

void FaviconWebStateDispatcherImpl::ReleaseAll() {
  web_states_.clear();
}

void FaviconWebStateDispatcherImpl::ReturnWebState(
    std::unique_ptr<web::WebState> web_state_unique) {
  web::WebState* web_state = web_state_unique.get();
  web_states_.push_back(std::move(web_state_unique));
  base::WeakPtr<FaviconWebStateDispatcherImpl> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  // This block will delete the web_state in keep_alive_second_ seconds.
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, keep_alive_second_ * NSEC_PER_SEC),
      dispatch_get_main_queue(), ^{
        FaviconWebStateDispatcherImpl* web_state_dispatcher = weak_this.get();
        if (web_state_dispatcher) {
          auto it = find_if(
              web_state_dispatcher->web_states_.begin(),
              web_state_dispatcher->web_states_.end(),
              [web_state](std::unique_ptr<web::WebState>& unique_web_state) {
                return unique_web_state.get() == web_state;
              });
          if (it != web_state_dispatcher->web_states_.end())
            web_state_dispatcher->web_states_.erase(it);
        }
      });
}

}  // namespace reading_list
