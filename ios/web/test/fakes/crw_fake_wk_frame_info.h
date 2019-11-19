// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_CRW_FAKE_WK_FRAME_INFO_H_
#define IOS_WEB_TEST_FAKES_CRW_FAKE_WK_FRAME_INFO_H_

#import <WebKit/WebKit.h>

// Fake WKFrameInfo class which can be used for testing.
@interface CRWFakeWKFrameInfo : WKFrameInfo
// Redefined WKNavigationAction properties as readwrite.
@property(nonatomic, readwrite, getter=isMainFrame) BOOL mainFrame;
@property(nonatomic, readwrite, copy) NSURLRequest* request;
@property(nonatomic, readwrite) WKSecurityOrigin* securityOrigin;
@property(nonatomic, readwrite, weak) WKWebView* webView;
@end

#endif  // IOS_WEB_TEST_FAKES_CRW_FAKE_WK_FRAME_INFO_H_
