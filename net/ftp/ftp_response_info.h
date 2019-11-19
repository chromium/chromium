// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_RESPONSE_INFO_H_
#define NET_FTP_FTP_RESPONSE_INFO_H_

#include <stdint.h>

#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"

namespace net {

class NET_EXPORT_PRIVATE FtpResponseInfo {
 public:
  FtpResponseInfo();
  ~FtpResponseInfo();

  // True if authentication failed and valid authentication credentials are
  // needed.
  bool needs_auth;

  // The time at which the request was made that resulted in this response.
  // For cached responses, this time could be "far" in the past.
  base::Time request_time;

  // The time at which the response headers were received.  For cached
  // responses, this time could be "far" in the past.
  base::Time response_time;

  // Expected content size, in bytes, as reported by SIZE command. Only valid
  // for file downloads. -1 means unknown size.
  int64_t expected_content_size;

  // True if the response data is of a directory listing.
  bool is_directory_listing;

  // Remote address of the socket which fetched this resource.
  IPEndPoint remote_endpoint;
};

}  // namespace net

#endif  // NET_FTP_FTP_RESPONSE_INFO_H_
