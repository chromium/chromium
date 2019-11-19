// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/dom_altering_lock.h"

#include "base/logging.h"
#include "ios/web/public/thread/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DOMAlteringLock::DOMAlteringLock(web::WebState* web_state) {
}

DOMAlteringLock::~DOMAlteringLock() {
}

void DOMAlteringLock::Acquire(id<DOMAltering> feature,
                              ProceduralBlockWithBool lockAction) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (current_dom_altering_feature_ == feature) {
    lockAction(YES);
    return;
  }
  if (current_dom_altering_feature_) {
    if (![current_dom_altering_feature_ canReleaseDOMLock]) {
      lockAction(NO);
      return;
    }
    [current_dom_altering_feature_ releaseDOMLockWithCompletionHandler:^{
      DCHECK_CURRENTLY_ON(web::WebThread::UI);
      DCHECK(current_dom_altering_feature_ == nil)
          << "The lock must be released before calling the completion handler.";
      current_dom_altering_feature_ = feature;
      lockAction(YES);
    }];
    return;
  }
  current_dom_altering_feature_ = feature;
  lockAction(YES);
}

// Release the lock on the DOM tree.
void DOMAlteringLock::Release(id<DOMAltering> feature) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (current_dom_altering_feature_ == feature)
    current_dom_altering_feature_ = nil;
}

WEB_STATE_USER_DATA_KEY_IMPL(DOMAlteringLock)
