// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/common/remoting_client.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "remoting/base/directory_service_client.h"
#include "remoting/base/oauth_token_info.h"
#include "remoting/base/passthrough_oauth_token_getter.h"
#include "remoting/client/common/logging.h"
#include "remoting/proto/remoting/v1/remote_support_host_messages.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

RemotingClient::RemotingClient(
    base::OnceClosure quit_closure,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : quit_closure_(std::move(quit_closure)),
      url_loader_factory_(url_loader_factory) {}

RemotingClient::~RemotingClient() = default;

void RemotingClient::StartSession(std::string_view support_id,
                                  std::string_view access_token) {
  oauth_token_getter_ = std::make_unique<PassthroughOAuthTokenGetter>(
      OAuthTokenInfo{std::string(access_token)});
  directory_service_client_ = std::make_unique<DirectoryServiceClient>(
      oauth_token_getter_.get(), url_loader_factory_);

  // base::Unretained is sound because this instance owns the service client
  // and callbacks will not be run after destruction.
  directory_service_client_->GetManagedChromeOsHost(
      std::string(support_id),
      base::BindOnce(&RemotingClient::OnGetManagedChromeOsHostRetrieved,
                     base::Unretained(this)));
}

void RemotingClient::OnGetManagedChromeOsHostRetrieved(
    const HttpStatus& status,
    std::unique_ptr<apis::v1::GetManagedChromeOsHostResponse> response) {
  CLIENT_LOG << "GetManagedChromeOsHost result: " << status.ok()
             << ", error: " << status.error_message();

  std::move(quit_closure_).Run();
}

}  // namespace remoting
