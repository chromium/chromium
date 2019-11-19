// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_SERVER_TYPE_H_
#define NET_FTP_FTP_SERVER_TYPE_H_

namespace net {

enum FtpServerType {
  // Cases in which we couldn't parse the server's response. That means
  // a server type we don't recognize, a security attack (when what we're
  // connecting to isn't an FTP server), or a broken server.
  SERVER_UNKNOWN = 0,

  SERVER_LS = 1,       // Server using /bin/ls -l listing style.
  SERVER_WINDOWS = 2,  // Server using Windows listing style.
  SERVER_VMS = 3,      // Server using VMS listing style.
  SERVER_NETWARE = 4,  // OBSOLETE. Server using Netware listing style.
  SERVER_OS2 = 5,      // OBSOLETE. Server using OS/2 listing style.

  NUM_OF_SERVER_TYPES
};

}  // namespace net

#endif  // NET_FTP_FTP_SERVER_TYPE_H_
