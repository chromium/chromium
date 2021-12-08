// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_FAKE_NATIVE_TASK_BRIDGE_H_
#define IOS_WEB_TEST_FAKES_FAKE_NATIVE_TASK_BRIDGE_H_

#import "ios/web/download/download_native_task_bridge.h"

// Used to simulate methods in NativeTaskBridge
@interface FakeNativeTaskBridge : DownloadNativeTaskBridge

// Used in testing to initialize ivars for a proper fake download and is only
// available in iOS 15+ as it uses |download|
- (void)downloadInitialized API_AVAILABLE(ios(15));

// Called in |_startDownloadBlock| in DownloadNativeTaskBridge to check if the
// block was called.
@property(nonatomic, readwrite) BOOL calledStartDownloadBlock;

// Overriding properties in NativeTaskBridge to be used in unit tests
@property(nonatomic, readwrite, strong)
    WKDownload* download API_AVAILABLE(ios(15));
@property(nonatomic, readwrite, strong) NSProgress* progress;
@property(nonatomic, readwrite, strong) NSURLResponse* response;
@property(nonatomic, readwrite, strong) NSString* suggestedFilename;

@end

#endif  // IOS_WEB_TEST_FAKES_FAKE_NATIVE_TASK_BRIDGE_H_
