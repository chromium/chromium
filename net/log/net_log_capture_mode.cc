// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_capture_mode.h"

#include <string>

#include "net/base/url_util.h"
#include "url/gurl.h"

namespace net {

bool NetLogCaptureIncludesSensitive(NetLogCaptureMode capture_mode) {
  return capture_mode >= NetLogCaptureMode::kIncludeSensitive;
}

bool NetLogCaptureIncludesSocketBytes(NetLogCaptureMode capture_mode) {
  return capture_mode == NetLogCaptureMode::kEverything;
}

std::string SanitizeUrlForNetLog(const GURL& url,
                                 NetLogCaptureMode capture_mode) {
  if (!url.is_valid() || (!url.has_username() && !url.has_password()) ||
      NetLogCaptureIncludesSensitive(capture_mode)) {
    return url.possibly_invalid_spec();
  }
  return RemoveCredentialsFromUrl(url).spec() + " (credentials redacted)";
}

}  // namespace net
