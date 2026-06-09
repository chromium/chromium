// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/model/url_loading_observer.h"

#import <ostream>

#import "base/check.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"

UrlLoadingObserver::UrlLoadingObserver() = default;

UrlLoadingObserver::~UrlLoadingObserver() {
  CHECK(!IsInObserverList()) << "UrlLoadingObserver must be removed from "
                                "WebState observer list before destruction.";
}

void UrlLoadingObserver::TabWillLoadUrl(
    const UrlLoadParams& params,
    base::WeakPtr<web::WebState> web_state) {
  TabWillLoadUrl(params.web_params.url, params.web_params.transition_type,
                 web_state);
}
