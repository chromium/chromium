// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_FAKE_NATIVE_TASK_BRIDGE_H_
#define IOS_WEB_TEST_FAKES_FAKE_NATIVE_TASK_BRIDGE_H_

#import "ios/web/download/download_native_task_bridge.h"

// Used to simulate methods in NativeTaskBridge
@interface FakeNativeTaskBridge : DownloadNativeTaskBridge

// Called in `_startDownloadBlock` in DownloadNativeTaskBridge to check if the
// block was called.
@property(nonatomic, readwrite) BOOL calledStartDownloadBlock;

// Overriding properties in NativeTaskBridge to be used in unit tests
@property(nonatomic, readwrite, strong) WKDownload* download;
@property(nonatomic, readwrite, strong) NSProgress* progress;
@property(nonatomic, readwrite, strong) NSURLResponse* response;
@property(nonatomic, readwrite, strong) NSString* suggestedFilename;

@end

#endif  // IOS_WEB_TEST_FAKES_FAKE_NATIVE_TASK_BRIDGE_H_
