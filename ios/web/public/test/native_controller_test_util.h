// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_NATIVE_CONTROLLER_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_NATIVE_CONTROLLER_TEST_UTIL_H_

@protocol CRWNativeContent;

namespace web {

class WebState;

namespace test {

// Returns the native controller of the given |web_state|.
id<CRWNativeContent> GetCurrentNativeController(WebState* web_state);

}  // namespace test
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_NATIVE_CONTROLLER_TEST_UTIL_H_
