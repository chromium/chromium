// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_CRW_FAKE_WK_NAVIGATION_ACTION_H_
#define IOS_WEB_TEST_FAKES_CRW_FAKE_WK_NAVIGATION_ACTION_H_

#import <WebKit/WebKit.h>

// Fake WKNavigationAction class which can be used for testing.
@interface CRWFakeWKNavigationAction : WKNavigationAction
// Redefined WKNavigationAction properties as readwrite.
@property(nullable, nonatomic, copy) WKFrameInfo* sourceFrame;
@property(nullable, nonatomic, copy) WKFrameInfo* targetFrame;
@property(nonatomic) WKNavigationType navigationType;
@property(nullable, nonatomic, copy) NSURLRequest* request;
@end

#endif  // IOS_WEB_TEST_FAKES_CRW_FAKE_WK_NAVIGATION_ACTION_H_
