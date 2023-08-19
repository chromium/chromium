// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/keyboard_observer_helper_app_interface.h"

#import "ios/chrome/browser/shared/ui/util/keyboard_observer_helper.h"

@implementation KeyboardObserverHelperAppInterface

+ (KeyboardObserverHelper*)appSharedInstance {
  return [KeyboardObserverHelper sharedKeyboardObserver];
}

@end
