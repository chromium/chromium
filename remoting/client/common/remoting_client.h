// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_COMMON_REMOTING_CLIENT_H_
#define REMOTING_CLIENT_COMMON_REMOTING_CLIENT_H_

#include <memory>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "remoting/base/http_status.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace remoting {

namespace apis::v1 {
class GetManagedChromeOsHostResponse;
}  // namespace apis::v1

class DirectoryServiceClient;
class PassthroughOAuthTokenGetter;

// A simple, native chromoting client implementation.
class RemotingClient {
 public:
  RemotingClient(
      base::OnceClosure quit_closure,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  RemotingClient(const RemotingClient&) = delete;
  RemotingClient& operator=(const RemotingClient&) = delete;

  ~RemotingClient();

  void StartSession(std::string_view support_id, std::string_view access_token);

 private:
  void OnGetManagedChromeOsHostRetrieved(
      const HttpStatus& status,
      std::unique_ptr<apis::v1::GetManagedChromeOsHostResponse> response);

  base::OnceClosure quit_closure_;

  // Used to provide an OAuth access token for service requests. Since a raw *
  // is passed around, this field should be destroyed after the service clients.
  std::unique_ptr<PassthroughOAuthTokenGetter> oauth_token_getter_;

  // Used to retrieve details about the remote host to connect to.
  std::unique_ptr<DirectoryServiceClient> directory_service_client_;

  // Used to make service requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_COMMON_REMOTING_CLIENT_H_
