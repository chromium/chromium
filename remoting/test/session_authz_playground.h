// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_SESSION_AUTHZ_PLAYGROUND_H_
#define REMOTING_TEST_SESSION_AUTHZ_PLAYGROUND_H_

#include <memory>

#include "base/run_loop.h"
#include "remoting/base/corp_session_authz_service_client.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/protobuf_http_status.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace remoting {

class SessionAuthzPlayground {
 public:
  SessionAuthzPlayground();
  ~SessionAuthzPlayground();

  SessionAuthzPlayground(const SessionAuthzPlayground&) = delete;
  SessionAuthzPlayground& operator=(const SessionAuthzPlayground&) = delete;

  void Start();

 private:
  void GenerateHostToken();
  void VerifySessionToken(const std::string& session_id);
  void ReauthorizeHost(const std::string& session_id,
                       const std::string& reauth_token);
  std::unique_ptr<OAuthTokenGetter> CreateOAuthTokenGetter(
      const base::FilePath& host_config_file_path);

  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<CorpSessionAuthzServiceClient> service_client_;
};

}  // namespace remoting

#endif  // REMOTING_TEST_SESSION_AUTHZ_PLAYGROUND_H_
