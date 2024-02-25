// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_monster_netlog_params.h"

#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_store.h"

namespace net {

base::Value::Dict NetLogCookieMonsterConstructorParams(bool persistent_store) {
  base::Value::Dict dict;
  dict.Set("persistent_store", persistent_store);
  return dict;
}

base::Value::Dict NetLogCookieMonsterCookieAdded(
    const CanonicalCookie* cookie,
    bool sync_requested,
    NetLogCaptureMode capture_mode) {
  if (!NetLogCaptureIncludesSensitive(capture_mode))
    return base::Value::Dict();

  base::Value::Dict dict;
  dict.Set("name", cookie->Name());
  dict.Set("value", cookie->Value());
  dict.Set("domain", cookie->Domain());
  dict.Set("path", cookie->Path());
  dict.Set("httponly", cookie->IsHttpOnly());
  dict.Set("secure", cookie->SecureAttribute());
  dict.Set("priority", CookiePriorityToString(cookie->Priority()));
  dict.Set("same_site", CookieSameSiteToString(cookie->SameSite()));
  dict.Set("is_persistent", cookie->IsPersistent());
  dict.Set("sync_requested", sync_requested);
  return dict;
}

base::Value::Dict NetLogCookieMonsterCookieDeleted(
    const CanonicalCookie* cookie,
    CookieChangeCause cause,
    bool sync_requested,
    NetLogCaptureMode capture_mode) {
  if (!NetLogCaptureIncludesSensitive(capture_mode))
    return base::Value::Dict();

  base::Value::Dict dict;
  dict.Set("name", cookie->Name());
  dict.Set("value", cookie->Value());
  dict.Set("domain", cookie->Domain());
  dict.Set("path", cookie->Path());
  dict.Set("is_persistent", cookie->IsPersistent());
  dict.Set("deletion_cause", CookieChangeCauseToString(cause));
  dict.Set("sync_requested", sync_requested);
  return dict;
}

base::Value::Dict NetLogCookieMonsterCookieRejectedSecure(
    const CanonicalCookie* old_cookie,
    const CanonicalCookie* new_cookie,
    NetLogCaptureMode capture_mode) {
  if (!NetLogCaptureIncludesSensitive(capture_mode))
    return base::Value::Dict();
  base::Value::Dict dict;
  dict.Set("name", old_cookie->Name());
  dict.Set("domain", old_cookie->Domain());
  dict.Set("oldpath", old_cookie->Path());
  dict.Set("newpath", new_cookie->Path());
  dict.Set("oldvalue", old_cookie->Value());
  dict.Set("newvalue", new_cookie->Value());
  return dict;
}

base::Value::Dict NetLogCookieMonsterCookieRejectedHttponly(
    const CanonicalCookie* old_cookie,
    const CanonicalCookie* new_cookie,
    NetLogCaptureMode capture_mode) {
  if (!NetLogCaptureIncludesSensitive(capture_mode))
    return base::Value::Dict();
  base::Value::Dict dict;
  dict.Set("name", old_cookie->Name());
  dict.Set("domain", old_cookie->Domain());
  dict.Set("path", old_cookie->Path());
  dict.Set("oldvalue", old_cookie->Value());
  dict.Set("newvalue", new_cookie->Value());
  return dict;
}

base::Value::Dict NetLogCookieMonsterCookiePreservedSkippedSecure(
    const CanonicalCookie* skipped_secure,
    const CanonicalCookie* preserved,
    const CanonicalCookie* new_cookie,
    NetLogCaptureMode capture_mode) {
  if (!NetLogCaptureIncludesSensitive(capture_mode))
    return base::Value::Dict();
  base::Value::Dict dict;
  dict.Set("name", preserved->Name());
  dict.Set("domain", preserved->Domain());
  dict.Set("path", preserved->Path());
  dict.Set("securecookiedomain", skipped_secure->Domain());
  dict.Set("securecookiepath", skipped_secure->Path());
  dict.Set("preservedvalue", preserved->Value());
  dict.Set("discardedvalue", new_cookie->Value());
  return dict;
}

}  // namespace net
