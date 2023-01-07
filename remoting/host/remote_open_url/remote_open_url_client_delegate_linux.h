// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_CLIENT_DELEGATE_LINUX_H_
#define REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_CLIENT_DELEGATE_LINUX_H_

#include <memory>

#include "remoting/host/remote_open_url/remote_open_url_client.h"

namespace base {
class Environment;
}  // namespace base

namespace remoting {

// RemoteOpenUrlClient::Delegate implementation for Linux.
class RemoteOpenUrlClientDelegateLinux final
    : public RemoteOpenUrlClient::Delegate {
 public:
  RemoteOpenUrlClientDelegateLinux();
  ~RemoteOpenUrlClientDelegateLinux() override;

  void OpenUrlOnFallbackBrowser(const GURL& url) override;
  void ShowOpenUrlError(const GURL& url) override;

 private:
  std::unique_ptr<base::Environment> environment_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_CLIENT_DELEGATE_LINUX_H_
