// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url_client.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "remoting/base/logging.h"
#include "remoting/host/host_setting_keys.h"
#include "remoting/host/host_settings.h"
#include "remoting/host/remote_open_url_constants.h"

namespace remoting {

namespace {

constexpr char kChromeRemoteDesktopSessionEnvVar[] =
    "CHROME_REMOTE_DESKTOP_SESSION";

void OpenOnFallbackBrowserInternal(const GURL& url = GURL()) {
  std::string previous_default_browser =
      HostSettings::GetInstance()->GetString(kLinuxPreviousDefaultWebBrowser);
  if (previous_default_browser.empty()) {
    LOG(ERROR) << "The previous default web browser is unknown.";
    return;
  }
  // gtk-launch DESKTOP_ENTRY [URL...]
  base::CommandLine gtk_launch_command(
      {"gtk-launch", previous_default_browser});
  if (!url.is_empty()) {
    gtk_launch_command.AppendArg(url.spec());
  }
  base::LaunchProcess(gtk_launch_command, base::LaunchOptions());
}

}  // namespace

RemoteOpenUrlClient::RemoteOpenUrlClient()
    : environment_(base::Environment::Create()) {}

RemoteOpenUrlClient::~RemoteOpenUrlClient() {
  DCHECK(!done_);
}

void RemoteOpenUrlClient::OpenFallbackBrowser() {
  OpenOnFallbackBrowserInternal();
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

  if (!environment_->HasVar(kChromeRemoteDesktopSessionEnvVar)) {
    LOG(WARNING) << "The program is not run on a remote session. "
                 << "Falling back to the previous default browser...";
    OnOpenUrlResponse(mojom::OpenUrlResult::LOCAL_FALLBACK);
    return;
  }

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
      OpenOnFallbackBrowserInternal(url_);
      break;
    default:
      NOTREACHED();
  }
  std::move(done_).Run();
}

}  // namespace remoting
