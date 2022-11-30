// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_request_config.h"

#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace remoting {

ProtobufHttpRequestConfig::ProtobufHttpRequestConfig(
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : traffic_annotation(traffic_annotation) {}

ProtobufHttpRequestConfig::~ProtobufHttpRequestConfig() = default;

void ProtobufHttpRequestConfig::Validate() const {
  DCHECK(request_message);
  DCHECK(!path.empty());
}

}  // namespace remoting
