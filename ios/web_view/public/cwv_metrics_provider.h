// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_METRICS_PROVIDER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_METRICS_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Provides metrics data from ios/web_view to be logged by the client.
CWV_EXPORT
@interface CWVMetricsProvider : NSObject

// Returns the singleton instance of this class.
@property(class, nonatomic, readonly) CWVMetricsProvider* sharedInstance;

- (instancetype)init NS_UNAVAILABLE;

// Returns the new metrics that have been collected since the last call to this
// method. The returned data is drained from the underlying storage, so it is up
// to the caller to ensure its lifetime as needed.
// The data is serialized according to the proto definition found at
// third_party/metrics_proto/chrome_user_metrics_extension.proto.
// This method is thread safe.
- (NSData*)consumeMetrics;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_METRICS_PROVIDER_H_
