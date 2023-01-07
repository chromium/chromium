// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_CRW_FAKE_NSURL_SESSION_TASK_H_
#define IOS_WEB_TEST_FAKES_CRW_FAKE_NSURL_SESSION_TASK_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Fake NSURLSessionDataTask class which can be used for testing. `cancel` and
// `resume` methods only change the `state` of this task without actually
// starting or stopping the download.
@interface CRWFakeNSURLSessionTask : NSURLSessionDataTask

// Redefined NSURLSessionTask properties as readwrite.
@property(nonatomic) int64_t countOfBytesReceived;
@property(nonatomic) int64_t countOfBytesExpectedToReceive;
@property(nonatomic) NSURLSessionTaskState state;
@property(nonatomic, nullable, copy) NSURLResponse* response;

- (nullable instancetype)initWithURL:(NSURL*)URL;  // NS_DESIGNATED_INITIALIZER;
// - (nonnull instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_TEST_FAKES_CRW_FAKE_NSURL_SESSION_TASK_H_
