// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_PRIVACY_MODE_H_
#define NET_BASE_PRIVACY_MODE_H_

namespace net {

// `PrivacyMode` corresponds to the fetch spec "credentials mode" and determines
// whether cookies, client certificates, and server HTTP auth credentials should
// be sent (for requests) or are enabled (when used by the various partitioning
// keys).
enum PrivacyMode {
  // Cookies and other credentials are enabled. Corresponds to the fetch spec
  // credentials mode of "include".
  PRIVACY_MODE_DISABLED = 0,

  // Cookies and server HTTP auth credentials are not enabled, but due to
  // http://crbug.com/775438, client certs are still enabled.
  PRIVACY_MODE_ENABLED = 1,

  // Same as PRIVACY_MODE_ENABLED but with client certs disabled. Corresponds to
  // the fetch spec credentials mode "omit".
  PRIVACY_MODE_ENABLED_WITHOUT_CLIENT_CERTS = 2,

  // Same as PRIVACY_MODE_ENABLED but partitioned HTTP cookies are still
  // enabled.
  PRIVACY_MODE_ENABLED_PARTITIONED_STATE_ALLOWED = 3,
};

const char* PrivacyModeToDebugString(PrivacyMode privacy_mode);

}  // namespace net

#endif  // NET_BASE_PRIVACY_MODE_H_
