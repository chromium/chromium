// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYBOARD_KEYBOARD_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYBOARD_KEYBOARD_API_H_

#import <UIKit/UIKit.h>

namespace ios {
namespace provider {

// Returns the keyboard window over which accessories can be presented.
UIWindow* GetKeyboardWindow();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYBOARD_KEYBOARD_API_H_
