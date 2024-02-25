// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/proto_util.h"

#import <ostream>
#import <type_traits>

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"

namespace web {

NSMutableDictionary<NSString*, NSString*>* HttpRequestHeadersFromProto(
    const proto::HttpHeaderListStorage& storage) {
  NSMutableDictionary<NSString*, NSString*>* headers =
      [[NSMutableDictionary alloc] initWithCapacity:storage.headers_size()];

  for (const proto::HttpHeaderStorage& header : storage.headers()) {
    NSString* key = base::SysUTF8ToNSString(header.name());
    NSString* val = base::SysUTF8ToNSString(header.value());

    headers[key] = val;
  }

  return headers;
}

void SerializeHttpRequestHeadersToProto(
    NSDictionary<NSString*, NSString*>* headers,
    proto::HttpHeaderListStorage& storage) {
  CHECK_NE(headers.count, 0u);

  for (NSString* key in headers) {
    proto::HttpHeaderStorage* header = storage.add_headers();
    header->set_name(base::SysNSStringToUTF8(key));
    header->set_value(base::SysNSStringToUTF8(headers[key]));
  }
}

}  // namespace web
