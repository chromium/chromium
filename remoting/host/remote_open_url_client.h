// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_OPEN_URL_CLIENT_H_
#define REMOTING_HOST_REMOTE_OPEN_URL_CLIENT_H_

#include <memory>

#include "base/callback.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/mojom/remote_url_opener.mojom.h"

namespace base {
class Environment;
}  // namespace base

namespace remoting {

// A helper to allow the standalone open URL binary to open a URL remotely and
// handle local fallback.
class RemoteOpenUrlClient final {
 public:
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

  std::unique_ptr<base::Environment> environment_;
  GURL url_;
  base::OnceClosure done_;
  mojo::Remote<mojom::RemoteUrlOpener> remote_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_OPEN_URL_CLIENT_H_