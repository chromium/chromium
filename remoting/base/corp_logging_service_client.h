// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CORP_LOGGING_SERVICE_CLIENT_H_
#define REMOTING_BASE_CORP_LOGGING_SERVICE_CLIENT_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "remoting/base/logging_service_client.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/protobuf_http_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

// A helper class that communicates with the corp logging service using the Corp
// API. This is not used for external users. For internal details, see
// go/crd-corp-logging.
class CorpLoggingServiceClient : public LoggingServiceClient {
 public:
  CorpLoggingServiceClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<OAuthTokenGetter> oauth_token_getter);
  ~CorpLoggingServiceClient() override;

  CorpLoggingServiceClient(const CorpLoggingServiceClient&) = delete;
  CorpLoggingServiceClient& operator=(const CorpLoggingServiceClient&) = delete;

  void ReportSessionDisconnected(
      const internal::ReportSessionDisconnectedRequestStruct& request,
      StatusCallback done) override;

 private:
  std::unique_ptr<OAuthTokenGetter> oauth_token_getter_;
  ProtobufHttpClient http_client_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CORP_LOGGING_SERVICE_CLIENT_H_
