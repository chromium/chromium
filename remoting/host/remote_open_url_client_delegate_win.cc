// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url_client_delegate_win.h"

#include "base/notreached.h"

namespace remoting {

RemoteOpenUrlClientDelegateWin::RemoteOpenUrlClientDelegateWin() = default;

RemoteOpenUrlClientDelegateWin::~RemoteOpenUrlClientDelegateWin() = default;

bool RemoteOpenUrlClientDelegateWin::IsInRemoteDesktopSession() {
  NOTIMPLEMENTED();
  return true;
}

void RemoteOpenUrlClientDelegateWin::OpenUrlOnFallbackBrowser(const GURL& url) {
  NOTIMPLEMENTED();
}

void RemoteOpenUrlClientDelegateWin::ShowOpenUrlError(const GURL& url) {
  NOTIMPLEMENTED();
}

}  // namespace remoting
