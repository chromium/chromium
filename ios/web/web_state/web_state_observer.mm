// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state_observer.h"

#import <ostream>

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

WebStateObserver::WebStateObserver() = default;

WebStateObserver::~WebStateObserver() {
  CHECK(!IsInObserverList()) << "WebStateObserver must be removed from "
                                "WebState observer list before destruction.";
}

}  // namespace web
