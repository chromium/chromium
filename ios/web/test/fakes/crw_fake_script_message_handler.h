// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_CRW_FAKE_SCRIPT_MESSAGE_HANDLER_H_
#define IOS_WEB_TEST_FAKES_CRW_FAKE_SCRIPT_MESSAGE_HANDLER_H_

#import <WebKit/WebKit.h>

// A class which handles receiving script message responses to store the last
// received WKScriptMessage instance.
@interface CRWFakeScriptMessageHandler : NSObject <WKScriptMessageHandler>
@property(nonatomic, strong) WKScriptMessage* lastReceivedScriptMessage;
@end

#endif  // IOS_WEB_TEST_FAKES_CRW_FAKE_SCRIPT_MESSAGE_HANDLER_H_
