// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_ITEM_STORAGE_TEST_UTIL_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_ITEM_STORAGE_TEST_UTIL_H_

#import <Foundation/Foundation.h>

@class CRWNavigationItemStorage;

namespace web {

// Returns whether `item1` is equivalent to `item2`.
BOOL ItemStoragesAreEqual(CRWNavigationItemStorage* item1,
                          CRWNavigationItemStorage* item2);
}

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_ITEM_STORAGE_TEST_UTIL_H_
