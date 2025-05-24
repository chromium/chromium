// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_USER_SCRIPT_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_USER_SCRIPT_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, CWVUserScriptInjectionTime) {
  CWVUserScriptInjectionTimeAtDocumentStart,
  CWVUserScriptInjectionTimeAtDocumentEnd
};

// User Script to be injected into a webpage.
CWV_EXPORT
@interface CWVUserScript : NSObject

// JavaScript source code.
@property(nonatomic, copy, readonly) NSString* source;

// Whether the script should be injected into all frames or just the main frame.
@property(nonatomic, readonly, getter=isForMainFrameOnly) BOOL forMainFrameOnly;

// The time at which the script should be injected: at the start or end of the
// document load.
@property(nonatomic, readonly) CWVUserScriptInjectionTime injectionTime;

- (instancetype)init NS_UNAVAILABLE;

// Creates a user script which should be injected into the main frame only at document start time.
- (instancetype)initWithSource:(NSString*)source;

// Creates a user script which should be injected into all frames or just the
// main frame at document start time.
- (instancetype)initWithSource:(NSString*)source
              forMainFrameOnly:(BOOL)forMainFrameOnly;

// Creates a user script which will be injected into all frames or just the
// main frame, and at the specified injection time.
- (instancetype)initWithSource:(NSString*)source
              forMainFrameOnly:(BOOL)forMainFrameOnly
                 injectionTime:(CWVUserScriptInjectionTime)injectionTime;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_USER_SCRIPT_H_
