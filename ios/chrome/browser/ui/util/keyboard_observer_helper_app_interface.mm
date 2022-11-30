// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/keyboard_observer_helper_app_interface.h"

#import "ios/chrome/browser/ui/util/keyboard_observer_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation KeyboardObserverHelperAppInterface

+ (KeyboardObserverHelper*)appSharedInstance {
  return [KeyboardObserverHelper sharedKeyboardObserver];
}

@end
