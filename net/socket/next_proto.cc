// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/next_proto.h"

#include <string_view>

namespace net {

NextProto NextProtoFromString(std::string_view proto_string) {
  if (proto_string == "http/1.1") {
    return NextProto::kProtoHTTP11;
  }
  if (proto_string == "h2") {
    return NextProto::kProtoHTTP2;
  }
  if (proto_string == "quic" || proto_string == "hq") {
    return NextProto::kProtoQUIC;
  }

  return NextProto::kProtoUnknown;
}

const char* NextProtoToString(NextProto next_proto) {
  switch (next_proto) {
    case NextProto::kProtoHTTP11:
      return "http/1.1";
    case NextProto::kProtoHTTP2:
      return "h2";
    case NextProto::kProtoQUIC:
      return "quic";
    case NextProto::kProtoUnknown:
      break;
  }
  return "unknown";
}

}  // namespace net
