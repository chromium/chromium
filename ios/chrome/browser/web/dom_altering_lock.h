// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_DOM_ALTERING_LOCK_H_
#define IOS_CHROME_BROWSER_WEB_DOM_ALTERING_LOCK_H_

#include "base/ios/block_types.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

typedef void (^ProceduralBlockWithBool)(BOOL);

// This protocol must be implemented by all classes which may alter the DOM tree
// of a web page. Before altering the DOM, the class must call
// DOMAlteringLock::Acquire() and can only proceed if the lock is really
// acquired.
// After restoring the DOM tree, the class must call DOMAlteringLock::Release().
@protocol DOMAltering<NSObject>

// Method called when another class wants to acquire the lock.
// Return YES if the class is ready to restore the DOM tree to its initial state
// and release the lock. A call to |releaseDOMLockWithCompletionHandler:|
// will follow to do the actual cleaning.
// Return NO if the class wants to keep an exclusive access to the DOM tree.
// Other features must account for the fact that they may not be able to acquire
// a lock on the DOM and behave accordingly.
- (BOOL)canReleaseDOMLock;

// Method called when another class wants to acquire the lock.
// The class must restore the DOM tree, call DOMAlteringLock::Release() and then
// |completionHandler|.
- (void)releaseDOMLockWithCompletionHandler:(ProceduralBlock)completionHandler;

@end

class DOMAlteringLock : public web::WebStateUserData<DOMAlteringLock> {
 public:
  DOMAlteringLock(web::WebState* web_state);
  ~DOMAlteringLock() override;

  // This method must be called before altering the DOM of the page. This will
  // ensure that only one class tries to alter the page at a time.
  // The completion handler is called with YES if the lock was acquired, or NO
  // if it could not.
  // This method must be called on the UI thread.
  void Acquire(id<DOMAltering> feature, ProceduralBlockWithBool lockAction);

  // Releases the lock on the DOM tree.
  // The lock is always released, even if it was acquired multiple times.
  // This method must be called on the UI thread.
  void Release(id<DOMAltering> feature);

 private:
  friend class web::WebStateUserData<DOMAlteringLock>;

  // DOMAltering object currently having the lock.
  __weak id<DOMAltering> current_dom_altering_feature_ = nil;
  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_WEB_DOM_ALTERING_LOCK_H_
