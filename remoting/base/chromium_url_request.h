// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CHROMIUM_URL_REQUEST_H_
#define REMOTING_BASE_CHROMIUM_URL_REQUEST_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/url_request.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
struct ResourceRequest;
}  // namespace network

namespace remoting {

// UrlRequest implementation based on network::SimpleURLLoader.
class ChromiumUrlRequest : public UrlRequest {
 public:
  ChromiumUrlRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      UrlRequest::Type type,
      const std::string& url,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  ~ChromiumUrlRequest() override;

  // UrlRequest interface.
  void AddHeader(const std::string& value) override;
  void SetPostData(const std::string& content_type,
                   const std::string& data) override;
  void Start(OnResultCallback on_result_callback) override;

 private:
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::ResourceRequest> resource_request_;

  const net::NetworkTrafficAnnotationTag traffic_annotation_;
  std::string post_data_content_type_;
  std::string post_data_;

  OnResultCallback on_result_callback_;
};

class ChromiumUrlRequestFactory : public UrlRequestFactory {
 public:
  ChromiumUrlRequestFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~ChromiumUrlRequestFactory() override;

  // UrlRequestFactory interface.
  std::unique_ptr<UrlRequest> CreateUrlRequest(
      UrlRequest::Type type,
      const std::string& url,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CHROMIUM_URL_REQUEST_H_
