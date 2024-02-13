// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CORP_SESSION_AUTHZ_SERVICE_CLIENT_H_
#define REMOTING_BASE_CORP_SESSION_AUTHZ_SERVICE_CLIENT_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/base/session_authz_service_client.h"
#include "remoting/proto/session_authz_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace remoting {

// A helper class that communicates with the SessionAuthz service using the Corp
// API. For internal details, see go/crd-sessionauthz.
class CorpSessionAuthzServiceClient : public SessionAuthzServiceClient {
 public:
  CorpSessionAuthzServiceClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<OAuthTokenGetter> oauth_token_getter);
  ~CorpSessionAuthzServiceClient() override;

  CorpSessionAuthzServiceClient(const CorpSessionAuthzServiceClient&) = delete;
  CorpSessionAuthzServiceClient& operator=(
      const CorpSessionAuthzServiceClient&) = delete;

  void GenerateHostToken(GenerateHostTokenCallback callback) override;
  void VerifySessionToken(
      const internal::VerifySessionTokenRequestStruct& request,
      VerifySessionTokenCallback callback) override;
  void ReauthorizeHost(const internal::ReauthorizeHostRequestStruct& request,
                       ReauthorizeHostCallback callback) override;

 private:
  template <typename CallbackType>
  void ExecuteRequest(
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      const std::string& path,
      std::unique_ptr<google::protobuf::MessageLite> request_message,
      CallbackType callback);

  std::unique_ptr<OAuthTokenGetter> oauth_token_getter_;
  ProtobufHttpClient http_client_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CORP_SESSION_AUTHZ_SERVICE_CLIENT_H_
