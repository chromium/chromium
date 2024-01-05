// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/web_contents_observer_test_utils.h"

namespace content {

NavigationStartObserver::NavigationStartObserver(WebContents* web_contents,
                                                 Callback callback)
    : WebContentsObserver(web_contents), callback_(std::move(callback)) {}
NavigationStartObserver::~NavigationStartObserver() = default;

void NavigationStartObserver::DidStartNavigation(NavigationHandle* navigation) {
  callback_.Run(navigation);
}

NavigationFinishObserver::NavigationFinishObserver(WebContents* web_contents,
                                                   Callback callback)
    : WebContentsObserver(web_contents), callback_(std::move(callback)) {}
NavigationFinishObserver::~NavigationFinishObserver() = default;

void NavigationFinishObserver::DidFinishNavigation(
    NavigationHandle* navigation) {
  callback_.Run(navigation);
}

}  // namespace content
