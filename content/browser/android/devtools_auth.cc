// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/devtools_auth.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/logging.h"

namespace content {

bool CanUserConnectToDevTools(
    const net::UnixDomainServerSocket::Credentials& credentials) {
  struct passwd* creds = getpwuid(credentials.user_id);
  if (!creds || !creds->pw_name) {
    LOG(WARNING) << "DevTools: can't obtain creds for uid "
        << credentials.user_id;
    return false;
  }
  if (credentials.group_id == credentials.user_id &&
      (strcmp("root", creds->pw_name) == 0 ||   // For rooted devices
       strcmp("shell", creds->pw_name) == 0 ||  // For non-rooted devices

       // From processes signed with the same key
       credentials.user_id == getuid())) {
    return true;
  }
  LOG(WARNING) << "DevTools: connection attempt from " << creds->pw_name;
  return false;
}

}  // namespace content
