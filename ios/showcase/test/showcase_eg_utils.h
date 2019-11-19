// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_TEST_SHOWCASE_EG_UTILS_H_
#define IOS_SHOWCASE_TEST_SHOWCASE_EG_UTILS_H_

#import <Foundation/Foundation.h>

namespace showcase_utils {

// Opens the screen named |name| on the Showcase home screen.
void Open(NSString* name);

// Returns to the Showcase home screen from a particular screen.
void Close();

}  // namespace showcase_utils

#endif  // IOS_SHOWCASE_TEST_SHOWCASE_EG_UTILS_H_
