// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_DBSC_REQUEST_H_
#define NET_DEVICE_BOUND_SESSIONS_DBSC_REQUEST_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "net/base/net_export.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/site_for_cookies.h"
#include "net/device_bound_sessions/refresh_result.h"
#include "net/device_bound_sessions/session_access.h"
#include "net/device_bound_sessions/session_key.h"
#include "net/device_bound_sessions/session_usage.h"
#include "url/gurl.h"

namespace net {
class IsolationInfo;
class URLRequest;
class NetLogWithSource;
class URLRequestContext;
class NetworkDelegate;
}  // namespace net

namespace net::device_bound_sessions {

// Device Bound Sessions should support both HTTPS and WSS traffic. The
// WebSocket spec says that the scheme should be rewritten by the time
// request URLs are considered by Device Bound Sessions, but Chrome does
// not implement this. Instead, we wrap URLRequests in this class to enforce at
// the type system that certain rewrites must happen.
class NET_EXPORT DbscRequest {
 public:
  explicit DbscRequest(URLRequest* request);
  DbscRequest(const DbscRequest&);
  DbscRequest& operator=(const DbscRequest&);
  DbscRequest(DbscRequest&&);
  DbscRequest& operator=(DbscRequest&&);
  ~DbscRequest();

  // Accessors that do not do WebSocket normalization.
  SessionUsage device_bound_session_usage() const;
  void set_device_bound_session_usage(
      device_bound_sessions::SessionUsage usage);
  const base::flat_map<SessionKey, RefreshResult>&
  device_bound_session_deferrals() const;
  base::RepeatingCallback<void(const SessionAccess&)>
  device_bound_session_access_callback();
  base::WeakPtr<URLRequest> GetWeakPtr();
  const NetLogWithSource& net_log() const;
  const std::optional<url::Origin>& initiator() const;
  const URLRequestContext* context() const;
  bool force_ignore_site_for_cookies() const;
  const SiteForCookies& site_for_cookies() const;
  const IsolationInfo& isolation_info() const;
  bool force_main_frame_for_same_site_cookies() const;
  const std::string& method() const;
  bool ignore_unsafe_method_for_same_site_lax() const;
  const CookieAccessResultList& maybe_sent_cookies() const;
  NetworkDelegate* network_delegate() const;
  bool allows_device_bound_session_registration() const;
  int load_flags() const;

  // Methods that need to do WebSocket normalization:
  const GURL& url() const;
  const std::vector<GURL>& url_chain() const;

  // Where possible, always use the getters above instead of using the
  // request directly.
  URLRequest* unnormalized_request() { return request_; }

 private:
  raw_ptr<URLRequest> request_;
  // These fields cache the result of scheme rewriting. They are mutable
  // so we can populate the cache in a const context.
  mutable std::optional<GURL> normalized_url_;
  mutable std::optional<std::vector<GURL>> normalized_url_chain_;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_DBSC_REQUEST_H_
