// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_CLIENT_DELEGATE_WIN_H_
#define REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_CLIENT_DELEGATE_WIN_H_

#include "base/win/scoped_com_initializer.h"
#include "remoting/host/remote_open_url/remote_open_url_client.h"

namespace remoting {

// RemoteOpenUrlClient::Delegate implementation for Windows.
class RemoteOpenUrlClientDelegateWin final
    : public RemoteOpenUrlClient::Delegate {
 public:
  RemoteOpenUrlClientDelegateWin();
  ~RemoteOpenUrlClientDelegateWin() override;

  void OpenUrlOnFallbackBrowser(const GURL& url) override;
  void ShowOpenUrlError(const GURL& url) override;

 private:
  base::win::ScopedCOMInitializer scoped_com_initializer_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_CLIENT_DELEGATE_WIN_H_
