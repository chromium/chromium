// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_OPEN_URL_CLIENT_H_
#define REMOTING_HOST_REMOTE_OPEN_URL_CLIENT_H_

#include <memory>

#include "base/callback.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "remoting/host/mojom/remote_url_opener.mojom.h"

namespace remoting {

// A helper to allow the standalone open URL binary to open a URL remotely and
// handle local fallback.
class RemoteOpenUrlClient final {
 public:
  // An interface to support platform-specific implementation.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    virtual bool IsInRemoteDesktopSession() = 0;

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

  // Opens the URL and calls |done| when done.
  void OpenUrl(const GURL& url, base::OnceClosure done);

  RemoteOpenUrlClient(const RemoteOpenUrlClient&) = delete;
  RemoteOpenUrlClient& operator=(const RemoteOpenUrlClient&) = delete;

 private:
  void OnOpenUrlResponse(mojom::OpenUrlResult result);
  void OnRequestTimeout();

  std::unique_ptr<Delegate> delegate_;
  base::OneShotTimer timeout_timer_;
  GURL url_;
  base::OnceClosure done_;
  mojo::IsolatedConnection connection_;
  mojo::Remote<mojom::RemoteUrlOpener> remote_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_OPEN_URL_CLIENT_H_
