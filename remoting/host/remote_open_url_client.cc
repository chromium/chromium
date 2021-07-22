// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "remoting/base/logging.h"
#include "remoting/host/remote_open_url_constants.h"

#if defined(OS_LINUX)
#include "remoting/host/remote_open_url_client_delegate_linux.h"
#elif defined(OS_WIN)
#include "remoting/host/remote_open_url_client_delegate_win.h"
#endif

namespace remoting {

namespace {

constexpr base::TimeDelta kRequestTimeout = base::TimeDelta::FromSeconds(5);

std::unique_ptr<RemoteOpenUrlClient::Delegate> CreateDelegate() {
#if defined(OS_LINUX)
  return std::make_unique<RemoteOpenUrlClientDelegateLinux>();
#elif defined(OS_WIN)
  return std::make_unique<RemoteOpenUrlClientDelegateWin>();
#else
  NOTREACHED();
  return nullptr;
#endif
}

}  // namespace

RemoteOpenUrlClient::RemoteOpenUrlClient() : delegate_(CreateDelegate()) {}

RemoteOpenUrlClient::~RemoteOpenUrlClient() {
  DCHECK(!done_);
}

void RemoteOpenUrlClient::OpenFallbackBrowser() {
  DCHECK(url_.is_empty());
  delegate_->OpenUrlOnFallbackBrowser(url_);
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

  if (!url_.SchemeIsHTTPOrHTTPS() && !url_.SchemeIs("mailto")) {
    HOST_LOG << "Unrecognized scheme. Failing back to the previous default "
             << "browser...";
    OnOpenUrlResponse(mojom::OpenUrlResult::LOCAL_FALLBACK);
    return;
  }

  if (!delegate_->IsInRemoteDesktopSession()) {
    LOG(WARNING) << "The program is not run on a remote session. "
                 << "Falling back to the previous default browser...";
    OnOpenUrlResponse(mojom::OpenUrlResult::LOCAL_FALLBACK);
    return;
  }

  auto endpoint = mojo::NamedPlatformChannel::ConnectToServer(
      GetRemoteOpenUrlIpcChannelName());
  if (!endpoint.is_valid()) {
    HOST_LOG << "Can't make IPC connection. URL forwarding is probably "
             << "disabled by the client.";
    OnOpenUrlResponse(mojom::OpenUrlResult::LOCAL_FALLBACK);
    return;
  }

  mojo::PendingRemote<mojom::RemoteUrlOpener> pending_remote(
      connection_.Connect(std::move(endpoint)), /* version= */ 0);
  if (!pending_remote.is_valid()) {
    LOG(WARNING) << "Invalid message pipe.";
    OnOpenUrlResponse(mojom::OpenUrlResult::FAILURE);
    return;
  }

  remote_.Bind(std::move(pending_remote));
  timeout_timer_.Start(FROM_HERE, kRequestTimeout, this,
                       &RemoteOpenUrlClient::OnRequestTimeout);
  remote_->OpenUrl(url_, base::BindOnce(&RemoteOpenUrlClient::OnOpenUrlResponse,
                                        base::Unretained(this)));
}

void RemoteOpenUrlClient::OnOpenUrlResponse(mojom::OpenUrlResult result) {
  timeout_timer_.AbandonAndStop();
  switch (result) {
    case mojom::OpenUrlResult::SUCCESS:
      HOST_LOG << "The URL is successfully opened on the client.";
      break;
    case mojom::OpenUrlResult::FAILURE: {
      delegate_->ShowOpenUrlError(url_);
      break;
    }
    case mojom::OpenUrlResult::LOCAL_FALLBACK:
      delegate_->OpenUrlOnFallbackBrowser(url_);
      break;
    default:
      NOTREACHED();
  }
  std::move(done_).Run();
}

void RemoteOpenUrlClient::OnRequestTimeout() {
  LOG(ERROR) << "Timed out waiting for OpenUrl response.";
  OnOpenUrlResponse(mojom::OpenUrlResult::LOCAL_FALLBACK);
}

}  // namespace remoting
