// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url/remote_open_url_client.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/filename_util.h"
#include "remoting/base/logging.h"
#include "remoting/host/chromoting_host_services_client.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"
#include "remoting/host/mojom/remote_url_opener.mojom.h"

#if BUILDFLAG(IS_LINUX)
#include "remoting/host/remote_open_url/remote_open_url_client_delegate_linux.h"
#elif BUILDFLAG(IS_WIN)
#include "remoting/host/remote_open_url/remote_open_url_client_delegate_win.h"
#endif

namespace remoting {

namespace {

constexpr base::TimeDelta kRequestTimeout = base::Seconds(5);

std::unique_ptr<RemoteOpenUrlClient::Delegate> CreateDelegate() {
#if BUILDFLAG(IS_LINUX)
  return std::make_unique<RemoteOpenUrlClientDelegateLinux>();
#elif BUILDFLAG(IS_WIN)
  return std::make_unique<RemoteOpenUrlClientDelegateWin>();
#else
  NOTREACHED();
#endif
}

}  // namespace

RemoteOpenUrlClient::RemoteOpenUrlClient()
    : RemoteOpenUrlClient(CreateDelegate(),
                          std::make_unique<ChromotingHostServicesClient>(),
                          kRequestTimeout) {}

RemoteOpenUrlClient::RemoteOpenUrlClient(
    std::unique_ptr<Delegate> delegate,
    std::unique_ptr<ChromotingHostServicesProvider> api_provider,
    base::TimeDelta request_timeout)
    : delegate_(std::move(delegate)),
      api_provider_(std::move(api_provider)),
      request_timeout_(request_timeout) {}

RemoteOpenUrlClient::~RemoteOpenUrlClient() {
  DCHECK(!done_);
}

void RemoteOpenUrlClient::OpenFallbackBrowser() {
  DCHECK(url_.is_empty());
  delegate_->OpenUrlOnFallbackBrowser(url_);
}

void RemoteOpenUrlClient::Open(const base::CommandLine::StringType& arg,
                               base::OnceClosure done) {
  DCHECK(url_.is_empty());
  DCHECK(!done_);
  DCHECK(!remote_);

  done_ = std::move(done);

#if BUILDFLAG(IS_WIN)
  GURL url(base::WideToUTF8(arg));
#else
  GURL url(arg);
#endif

  if (!url.is_valid()) {
    // It has been observed that in some Linux environments, a file:// URL might
    // be converted back to a file path when it's being passed to
    // remote_open_url, so we try to convert it back to a file:// URL and open
    // it with the fallback browser.
    base::FilePath file_path(arg);
    if (file_path.IsAbsolute()) {
      HOST_LOG << "Argument appears to be an absolute file path. Will convert "
                  "it to a file:// URL.";
      url = net::FilePathToFileURL(file_path);
    }
  }

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

  auto* api = api_provider_->GetSessionServices();
  if (!api) {
    HOST_LOG << "Can't make IPC connection. The host is probably not running.";
    OnOpenUrlResponse(mojom::OpenUrlResult::LOCAL_FALLBACK);
    return;
  }
  api->BindRemoteUrlOpener(remote_.BindNewPipeAndPassReceiver());
  remote_.set_disconnect_handler(base::BindOnce(
      &RemoteOpenUrlClient::OnIpcDisconnected, base::Unretained(this)));
  timeout_timer_.Start(FROM_HERE, request_timeout_, this,
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
  remote_.reset();
}

void RemoteOpenUrlClient::OnRequestTimeout() {
  LOG(ERROR) << "Timed out waiting for OpenUrl response.";
  OnOpenUrlResponse(mojom::OpenUrlResult::LOCAL_FALLBACK);
}

void RemoteOpenUrlClient::OnIpcDisconnected() {
  LOG(WARNING) << "IPC disconnected.";
  // This generally happens either because the session is not remoted, or the
  // client hasn't enabled URL forwarding, so we fallback locally.
  OnOpenUrlResponse(mojom::OpenUrlResult::LOCAL_FALLBACK);
}

}  // namespace remoting
