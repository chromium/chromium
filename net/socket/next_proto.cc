// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/next_proto.h"

#include <string_view>

namespace net {

NextProto NextProtoFromString(std::string_view proto_string) {
  if (proto_string == "http/1.1") {
    return kProtoHTTP11;
  }
  if (proto_string == "h2") {
    return kProtoHTTP2;
  }
  if (proto_string == "quic" || proto_string == "hq") {
    return kProtoQUIC;
  }

  return kProtoUnknown;
}

const char* NextProtoToString(NextProto next_proto) {
  switch (next_proto) {
    case kProtoHTTP11:
      return "http/1.1";
    case kProtoHTTP2:
      return "h2";
    case kProtoQUIC:
      return "quic";
    case kProtoUnknown:
      break;
  }
  return "unknown";
}

}  // namespace net
