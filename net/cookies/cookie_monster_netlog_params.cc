// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_monster_netlog_params.h"

#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_store.h"

namespace net {

base::Value NetLogCookieMonsterConstructorParams(bool persistent_store) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetBoolKey("persistent_store", persistent_store);
  return dict;
}

base::Value NetLogCookieMonsterCookieAdded(const CanonicalCookie* cookie,
                                           bool sync_requested,
                                           NetLogCaptureMode capture_mode) {
  if (!NetLogCaptureIncludesSensitive(capture_mode))
    return base::Value();

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("name", cookie->Name());
  dict.SetStringKey("value", cookie->Value());
  dict.SetStringKey("domain", cookie->Domain());
  dict.SetStringKey("path", cookie->Path());
  dict.SetBoolKey("httponly", cookie->IsHttpOnly());
  dict.SetBoolKey("secure", cookie->IsSecure());
  dict.SetStringKey("priority", CookiePriorityToString(cookie->Priority()));
  dict.SetStringKey("same_site", CookieSameSiteToString(cookie->SameSite()));
  dict.SetBoolKey("is_persistent", cookie->IsPersistent());
  dict.SetBoolKey("sync_requested", sync_requested);
  return dict;
}

base::Value NetLogCookieMonsterCookieDeleted(const CanonicalCookie* cookie,
                                             CookieChangeCause cause,
                                             bool sync_requested,
                                             NetLogCaptureMode capture_mode) {
  if (!NetLogCaptureIncludesSensitive(capture_mode))
    return base::Value();

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("name", cookie->Name());
  dict.SetStringKey("value", cookie->Value());
  dict.SetStringKey("domain", cookie->Domain());
  dict.SetStringKey("path", cookie->Path());
  dict.SetBoolKey("is_persistent", cookie->IsPersistent());
  dict.SetStringKey("deletion_cause", CookieChangeCauseToString(cause));
  dict.SetBoolKey("sync_requested", sync_requested);
  return dict;
}

base::Value NetLogCookieMonsterCookieRejectedSecure(
    const CanonicalCookie* old_cookie,
    const CanonicalCookie* new_cookie,
    NetLogCaptureMode capture_mode) {
  if (!NetLogCaptureIncludesSensitive(capture_mode))
    return base::Value();
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("name", old_cookie->Name());
  dict.SetStringKey("domain", old_cookie->Domain());
  dict.SetStringKey("oldpath", old_cookie->Path());
  dict.SetStringKey("newpath", new_cookie->Path());
  dict.SetStringKey("oldvalue", old_cookie->Value());
  dict.SetStringKey("newvalue", new_cookie->Value());
  return dict;
}

base::Value NetLogCookieMonsterCookieRejectedHttponly(
    const CanonicalCookie* old_cookie,
    const CanonicalCookie* new_cookie,
    NetLogCaptureMode capture_mode) {
  if (!NetLogCaptureIncludesSensitive(capture_mode))
    return base::Value();
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("name", old_cookie->Name());
  dict.SetStringKey("domain", old_cookie->Domain());
  dict.SetStringKey("path", old_cookie->Path());
  dict.SetStringKey("oldvalue", old_cookie->Value());
  dict.SetStringKey("newvalue", new_cookie->Value());
  return dict;
}

base::Value NetLogCookieMonsterCookiePreservedSkippedSecure(
    const CanonicalCookie* skipped_secure,
    const CanonicalCookie* preserved,
    const CanonicalCookie* new_cookie,
    NetLogCaptureMode capture_mode) {
  if (!NetLogCaptureIncludesSensitive(capture_mode))
    return base::Value();
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("name", preserved->Name());
  dict.SetStringKey("domain", preserved->Domain());
  dict.SetStringKey("path", preserved->Path());
  dict.SetStringKey("securecookiedomain", skipped_secure->Domain());
  dict.SetStringKey("securecookiepath", skipped_secure->Path());
  dict.SetStringKey("preservedvalue", preserved->Value());
  dict.SetStringKey("discardedvalue", new_cookie->Value());
  return dict;
}

}  // namespace net
