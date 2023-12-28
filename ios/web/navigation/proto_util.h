// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_PROTO_UTIL_H_
#define IOS_WEB_NAVIGATION_PROTO_UTIL_H_

#import <Foundation/Foundation.h>

#include "ios/web/public/session/proto/navigation.pb.h"

namespace web {

// Creates a mutable HTTP request headers dictionary from serialized `storage`.
NSMutableDictionary<NSString*, NSString*>* HttpRequestHeadersFromProto(
    const proto::HttpHeaderListStorage& storage);

// Serializes `headers` into `storage`.
void SerializeHttpRequestHeadersToProto(
    NSDictionary<NSString*, NSString*>* headers,
    proto::HttpHeaderListStorage& storage);

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_PROTO_UTIL_H_
