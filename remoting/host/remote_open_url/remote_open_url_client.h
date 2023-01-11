// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_CLIENT_H_
#define REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_CLIENT_H_

#include <memory>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/mojom/remote_url_opener.mojom.h"
#include "url/gurl.h"

namespace remoting {

class ChromotingHostServicesProvider;

// A helper to allow the standalone open URL binary to open a URL remotely and
// handle local fallback.
class RemoteOpenUrlClient final {
 public:
  // An interface to support platform-specific implementation.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    // Opens |url| on the fallback browser. If |url| is empty, simply opens the
    // browser without a URL.
    virtual void OpenUrlOnFallbackBrowser(const GURL& url) = 0;

    // Shows an error message that indicates that |url| fails to be opened
    // remotely.
    virtual void ShowOpenUrlError(const GURL& url) = 0;
  };

  RemoteOpenUrlClient();
  ~RemoteOpenUrlClient();

  // Simply opens the fallback browser with no arguments.
  void OpenFallbackBrowser();

  // Opens |arg| (which can be either a URL or an absolute file path) and calls
  // |done| when done.
  void Open(const base::CommandLine::StringType& arg, base::OnceClosure done);

  RemoteOpenUrlClient(const RemoteOpenUrlClient&) = delete;
  RemoteOpenUrlClient& operator=(const RemoteOpenUrlClient&) = delete;

 private:
  friend class RemoteOpenUrlClientTest;

  // Ctor for unittests.
  RemoteOpenUrlClient(
      std::unique_ptr<Delegate> delegate,
      std::unique_ptr<ChromotingHostServicesProvider> api_provider,
      base::TimeDelta request_timeout);

  void OnOpenUrlResponse(mojom::OpenUrlResult result);
  void OnRequestTimeout();
  void OnIpcDisconnected();

  std::unique_ptr<Delegate> delegate_;
  std::unique_ptr<ChromotingHostServicesProvider> api_provider_;
  base::TimeDelta request_timeout_;
  base::OneShotTimer timeout_timer_;
  GURL url_;
  base::OnceClosure done_;
  mojo::Remote<mojom::RemoteUrlOpener> remote_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_CLIENT_H_
