// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_MONSTER_NETLOG_PARAMS_H_
#define NET_COOKIES_COOKIE_MONSTER_NETLOG_PARAMS_H_

#include "base/values.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/log/net_log_capture_mode.h"

namespace net {

// Returns a Value containing NetLog parameters for constructing
// a CookieMonster.
base::Value::Dict NetLogCookieMonsterConstructorParams(bool persistent_store);

// Returns a Value containing NetLog parameters for adding a cookie.
base::Value::Dict NetLogCookieMonsterCookieAdded(
    const CanonicalCookie* cookie,
    bool sync_requested,
    NetLogCaptureMode capture_mode);

// Returns a Value containing NetLog parameters for deleting a cookie.
base::Value::Dict NetLogCookieMonsterCookieDeleted(
    const CanonicalCookie* cookie,
    CookieChangeCause cause,
    bool sync_requested,
    NetLogCaptureMode capture_mode);

// Returns a Value containing NetLog parameters for when a cookie addition
// is rejected because of a conflict with a secure cookie.
base::Value::Dict NetLogCookieMonsterCookieRejectedSecure(
    const CanonicalCookie* old_cookie,
    const CanonicalCookie* new_cookie,
    NetLogCaptureMode capture_mode);

// Returns a Value containing NetLog parameters for when a cookie addition
// is rejected because of a conflict with an httponly cookie.
base::Value::Dict NetLogCookieMonsterCookieRejectedHttponly(
    const CanonicalCookie* old_cookie,
    const CanonicalCookie* new_cookie,
    NetLogCaptureMode capture_mode);

// Returns a Value containing NetLog parameters for when, upon an attempted
// cookie addition which is rejected due to a conflict with a secure cookie, a
// pre-existing cookie would have been deleted but is instead preserved because
// the addition failed.
base::Value::Dict NetLogCookieMonsterCookiePreservedSkippedSecure(
    const CanonicalCookie* skipped_secure,
    const CanonicalCookie* preserved,
    const CanonicalCookie* new_cookie,
    NetLogCaptureMode capture_mode);

}  // namespace net

#endif  // NET_COOKIES_COOKIE_MONSTER_NETLOG_PARAMS_H_
