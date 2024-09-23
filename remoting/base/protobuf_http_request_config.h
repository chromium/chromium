// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PROTOBUF_HTTP_REQUEST_CONFIG_H_
#define REMOTING_BASE_PROTOBUF_HTTP_REQUEST_CONFIG_H_

#include <memory>
#include <string>

#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace remoting {

// Common configurations for unary and stream protobuf http requests. Caller
// needs to set all fields in this struct unless otherwise documented.
struct ProtobufHttpRequestConfig {
  explicit ProtobufHttpRequestConfig(
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  ~ProtobufHttpRequestConfig();

  // Runs DCHECK's on the fields to make sure all fields have been set up.
  void Validate() const;

  const net::NetworkTrafficAnnotationTag traffic_annotation;
  std::unique_ptr<google::protobuf::MessageLite> request_message;
  std::string path;
  bool authenticated = true;
  bool provide_certificate = false;
  std::string method = net::HttpRequestHeaders::kPostMethod;

  // Optional. Only needed when the request requires an API key.
  std::string api_key;
};

}  // namespace remoting

#endif  // REMOTING_BASE_PROTOBUF_HTTP_REQUEST_CONFIG_H_
