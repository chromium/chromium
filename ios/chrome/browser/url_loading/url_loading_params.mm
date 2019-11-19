// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/url_loading_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UrlLoadParams UrlLoadParams::InCurrentTab(
    const web::NavigationManager::WebLoadParams& web_params) {
  UrlLoadParams params = UrlLoadParams();
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.web_params = web_params;
  return params;
}

UrlLoadParams UrlLoadParams::InCurrentTab(const GURL& url,
                                          const GURL& virtual_url) {
  UrlLoadParams params = UrlLoadParams();
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.web_params = web::NavigationManager::WebLoadParams(url);
  params.web_params.virtual_url = virtual_url;
  return params;
}

UrlLoadParams UrlLoadParams::InCurrentTab(const GURL& url) {
  UrlLoadParams params = UrlLoadParams();
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.web_params = web::NavigationManager::WebLoadParams(url);
  return params;
}

UrlLoadParams UrlLoadParams::InNewTab(
    const web::NavigationManager::WebLoadParams& web_params) {
  UrlLoadParams params = UrlLoadParams();
  params.web_params = web_params;
  return params;
}

UrlLoadParams UrlLoadParams::InNewTab(const GURL& url,
                                      const GURL& virtual_url) {
  UrlLoadParams params = UrlLoadParams();
  params.web_params = web::NavigationManager::WebLoadParams(url);
  params.web_params.virtual_url = virtual_url;
  return params;
}

UrlLoadParams UrlLoadParams::InNewTab(const GURL& url) {
  UrlLoadParams params = UrlLoadParams();
  params.web_params = web::NavigationManager::WebLoadParams(url);
  return params;
}

UrlLoadParams UrlLoadParams::SwitchToTab(
    const web::NavigationManager::WebLoadParams& web_params) {
  UrlLoadParams params = UrlLoadParams();
  params.disposition = WindowOpenDisposition::SWITCH_TO_TAB;
  params.web_params = web_params;
  return params;
}

UrlLoadParams::UrlLoadParams()
    : web_params(GURL()),
      disposition(WindowOpenDisposition::NEW_FOREGROUND_TAB),
      in_incognito(false),
      append_to(kLastTab),
      origin_point(CGPointZero),
      from_chrome(false),
      user_initiated(true),
      should_focus_omnibox(false),
      load_strategy(UrlLoadStrategy::NORMAL) {}

UrlLoadParams::UrlLoadParams(const UrlLoadParams& other)
    : web_params(other.web_params),
      disposition(other.disposition),
      in_incognito(other.in_incognito),
      append_to(other.append_to),
      origin_point(other.origin_point),
      from_chrome(other.from_chrome),
      user_initiated(other.user_initiated),
      should_focus_omnibox(other.should_focus_omnibox),
      load_strategy(other.load_strategy) {}

UrlLoadParams& UrlLoadParams::operator=(const UrlLoadParams& other) {
  web_params = other.web_params;
  disposition = other.disposition;
  in_incognito = other.in_incognito;
  append_to = other.append_to;
  origin_point = other.origin_point;
  from_chrome = other.from_chrome;
  user_initiated = other.user_initiated;
  should_focus_omnibox = other.should_focus_omnibox;
  load_strategy = other.load_strategy;
  return *this;
}

void UrlLoadParams::SetInBackground(bool in_background) {
  this->disposition = in_background ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                                    : WindowOpenDisposition::NEW_FOREGROUND_TAB;
}
