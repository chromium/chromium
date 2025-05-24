// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CORP_SESSION_AUTHZ_SERVICE_CLIENT_H_
#define REMOTING_BASE_CORP_SESSION_AUTHZ_SERVICE_CLIENT_H_

#include <memory>
#include <string>
#include <string_view>

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
  // |support_id|: The 7-digit support ID. Empty implies that the connection
  //   mode is remote access.
  CorpSessionAuthzServiceClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<net::ClientCertStore> client_cert_store,
      std::unique_ptr<OAuthTokenGetter> oauth_token_getter,
      std::string_view support_id);
  ~CorpSessionAuthzServiceClient() override;

  CorpSessionAuthzServiceClient(const CorpSessionAuthzServiceClient&) = delete;
  CorpSessionAuthzServiceClient& operator=(
      const CorpSessionAuthzServiceClient&) = delete;

  void GenerateHostToken(GenerateHostTokenCallback callback) override;
  void VerifySessionToken(std::string_view session_token,
                          VerifySessionTokenCallback callback) override;
  void ReauthorizeHost(std::string_view session_reauth_token,
                       std::string_view session_id,
                       ReauthorizeHostCallback callback) override;

 private:
  template <typename CallbackType>
  void ExecuteRequest(
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      std::string_view verb,
      std::unique_ptr<google::protobuf::MessageLite> request_message,
      CallbackType callback);

  std::unique_ptr<OAuthTokenGetter> oauth_token_getter_;
  ProtobufHttpClient http_client_;
  std::string support_id_;
  std::string_view session_authz_path_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CORP_SESSION_AUTHZ_SERVICE_CLIENT_H_
