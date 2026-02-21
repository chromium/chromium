// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TESTS_HOOK_HELPER_H_
#define IOS_CHROME_APP_TESTS_HOOK_HELPER_H_

#import "ios/chrome/browser/shared/model/browser/browser.h"

// Creates and inserts fake tabs into the WebStateList of `browser` until the
// list reaches `count` items. This is only used for testing purposes.
void InjectUnrealizedWebStatesUntilListHasSizeItems(Browser* browser,
                                                    int count);

#endif  // IOS_CHROME_APP_TESTS_HOOK_HELPER_H_
