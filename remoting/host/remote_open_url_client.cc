// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "remoting/base/logging.h"
#include "remoting/host/remote_open_url_constants.h"

namespace remoting {

RemoteOpenUrlClient::RemoteOpenUrlClient() = default;

RemoteOpenUrlClient::~RemoteOpenUrlClient() {
  DCHECK(!done_);
}

void RemoteOpenUrlClient::OpenUrl(const GURL& url, base::OnceClosure done) {
  DCHECK(url_.is_empty());
  DCHECK(!done_);
  DCHECK(!remote_);

  done_ = std::move(done);

  if (!url.is_valid()) {
    LOG(ERROR) << "Invalid URL";
    OnOpenUrlResponse(mojom::OpenUrlResult::FAILURE);
    return;
  }

  url_ = url;

  auto endpoint = mojo::NamedPlatformChannel::ConnectToServer(
      GetRemoteOpenUrlIpcChannelName());
  if (!endpoint.is_valid()) {
    LOG(WARNING)
        << "Can't make IPC connection. The remote session may be disconnected.";
    OnOpenUrlResponse(mojom::OpenUrlResult::FAILURE);
    return;
  }

  auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
  mojo::PendingRemote<mojom::RemoteUrlOpener> pending_remote(
      invitation.ExtractMessagePipe(0), 0);
  if (!pending_remote.is_valid()) {
    LOG(WARNING) << "Invalid message pipe.";
    OnOpenUrlResponse(mojom::OpenUrlResult::FAILURE);
    return;
  }

  remote_.Bind(std::move(pending_remote));
  remote_->OpenUrl(url_, base::BindOnce(&RemoteOpenUrlClient::OnOpenUrlResponse,
                                        base::Unretained(this)));
}

void RemoteOpenUrlClient::OnOpenUrlResponse(mojom::OpenUrlResult result) {
  switch (result) {
    case mojom::OpenUrlResult::SUCCESS:
      HOST_LOG << "The URL is successfully opened on the client.";
      break;
    case mojom::OpenUrlResult::FAILURE:
      LOG(ERROR) << "Failed to open the URL remotely.";
      break;
    case mojom::OpenUrlResult::LOCAL_FALLBACK:
      NOTIMPLEMENTED();
      break;
    default:
      NOTREACHED();
  }
  std::move(done_).Run();
}

}  // namespace remoting
