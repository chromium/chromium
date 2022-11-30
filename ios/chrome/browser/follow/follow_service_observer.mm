// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/follow_service_observer.h"

#import <ostream>

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FollowServiceObserver::FollowServiceObserver() = default;

FollowServiceObserver::~FollowServiceObserver() {
  DCHECK(!IsInObserverList())
      << "FollowServiceObserver needs to be unregistered before destruction!";
}
