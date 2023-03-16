// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PARTIAL_TRANSLATE_PARTIAL_TRANSLATE_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_PARTIAL_TRANSLATE_PARTIAL_TRANSLATE_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// App interface to interact with the Edit Menu.
@interface PartialTranslateAppInterface : NSObject

// Returns whether the partial translate is enabled.
+ (BOOL)installedPartialTranslate;

@end

#endif  // IOS_CHROME_BROWSER_UI_PARTIAL_TRANSLATE_PARTIAL_TRANSLATE_APP_INTERFACE_H_
