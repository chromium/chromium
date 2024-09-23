// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/cookies/system_cookie_store.h"

#include <memory>

#include "base/logging.h"
#import "ios/net/cookies/cookie_creation_time_manager.h"
#include "ios/net/ios_net_buildflags.h"

namespace net {

SystemCookieStore::~SystemCookieStore() = default;

SystemCookieStore::SystemCookieStore()
    : creation_time_manager_(std::make_unique<CookieCreationTimeManager>()),
      weak_factory_(this) {}

void SystemCookieStore::SetCookieAsync(NSHTTPCookie* cookie,
                                       SystemCookieCallback callback) {
  SetCookieAsync(cookie, /*optional_creation_time=*/nullptr,
                 std::move(callback));
}

base::Time SystemCookieStore::GetCookieCreationTime(NSHTTPCookie* cookie) {
  return creation_time_manager_->GetCreationTime(cookie);
}

base::WeakPtr<SystemCookieStore> SystemCookieStore::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// protected static
NSInteger SystemCookieStore::CompareCookies(NSHTTPCookie* cookie_a,
                                            NSHTTPCookie* cookie_b,
                                            void* context) {
  DCHECK(context);
  // Compare path lengths first.
  NSUInteger path_length_a = cookie_a.path.length;
  NSUInteger path_length_b = cookie_b.path.length;
  if (path_length_a < path_length_b)
    return NSOrderedDescending;
  if (path_length_b < path_length_a)
    return NSOrderedAscending;

  // Compare creation times.
  CookieCreationTimeManager* manager =
      static_cast<CookieCreationTimeManager*>(context);
  base::Time created_a = manager->GetCreationTime(cookie_a);
  base::Time created_b = manager->GetCreationTime(cookie_b);
#if !BUILDFLAG(CRONET_BUILD)
  // CookieCreationTimeManager is returning creation times that are null.
  // Since in Cronet, the cookie store is recreated on startup, let's suppress
  // this warning for now.
  DLOG_IF(ERROR, created_a.is_null() || created_b.is_null())
      << "Cookie without creation date";
#endif
  if (created_a < created_b)
    return NSOrderedAscending;
  if (created_a > created_b)
    return NSOrderedDescending;

  return NSOrderedSame;
}

}  // namespace net
