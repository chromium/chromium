// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_DEVTOOLS_AUTH_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_DEVTOOLS_AUTH_H_

#include "content/common/content_export.h"
#include "net/socket/unix_domain_server_socket_posix.h"

namespace content {

// Returns true if the given peer identified by the credentials is authorized
// to connect to the devtools server, false if not.
CONTENT_EXPORT bool CanUserConnectToDevTools(
    const net::UnixDomainServerSocket::Credentials& credentials);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_DEVTOOLS_AUTH_H_
