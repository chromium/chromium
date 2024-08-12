// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/cookies/cookie_creation_time_manager.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/check_op.h"
#import "base/containers/contains.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "ios/net/ios_net_buildflags.h"

// Key holding the creation-time in NSHTTPCookie properties.
// This key is undocumented, and its value has type NSNumber.
NSString* const kHTTPCookieCreated = @"Created";

namespace {

// Gets the creation date of a NSHTTPCookie, reading a undocumented property.
base::Time GetCreationTimeFromObject(NSHTTPCookie* cookie) {
  // The "Created" key is not documented.
  // Return a null time if the key is missing.
  id created = [[cookie properties] objectForKey:kHTTPCookieCreated];
#if !BUILDFLAG(CRONET_BUILD)
  // In Cronet the cookie store is recreated on startup, so |created| could be
  // nil.
  DCHECK(created && [created isKindOfClass:[NSNumber class]]);
#endif
  if (!created || ![created isKindOfClass:[NSNumber class]])
    return base::Time();
  // created is the time from January 1st, 2001 in seconds.
  CFAbsoluteTime absolute_time = [(NSNumber*)created doubleValue];
  // If the cookie has been created using |-initWithProperties:|, the creation
  // date property is (incorrectly) set to 1.0. Treat that as an invalid time.
  if (absolute_time < 2.0)
    return base::Time();
  return base::Time::FromCFAbsoluteTime(absolute_time);
}

// Gets a string that can be used as a unique identifier for |cookie|.
std::string GetCookieUniqueID(NSHTTPCookie* cookie) {
  return base::SysNSStringToUTF8([cookie name]) + ";" +
         base::SysNSStringToUTF8([cookie domain]) + ";" +
         base::SysNSStringToUTF8([cookie path]);
}

}  // namespace

namespace net {

CookieCreationTimeManager::CookieCreationTimeManager() : weak_factory_(this) {
  DETACH_FROM_THREAD(thread_checker_);
}

CookieCreationTimeManager::~CookieCreationTimeManager() {
}

void CookieCreationTimeManager::SetCreationTime(
    NSHTTPCookie* cookie,
    const base::Time& creation_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!base::Contains(unique_times_, creation_time));

  // If the cookie overrides an existing cookie, remove its creation time.
  auto it = creation_times_.find(GetCookieUniqueID(cookie));
  if (it != creation_times_.end()) {
    size_t erased = unique_times_.erase(it->second);
    DCHECK_EQ(1u, erased);
  }

  unique_times_.insert(creation_time);
  creation_times_[GetCookieUniqueID(cookie)] = creation_time;
}

base::Time CookieCreationTimeManager::MakeUniqueCreationTime(
    const base::Time& creation_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = unique_times_.find(creation_time);

  if (it == unique_times_.end())
    return creation_time;

  // If the time already exist, increment until we find a time available.
  // |unique_times_| is sorted according to time, so we can traverse it from
  // |it|.
  base::Time time = creation_time;
  do {
    DCHECK(time == *it);
    ++it;
    time = base::Time::FromInternalValue(time.ToInternalValue() + 1);
  } while (it != unique_times_.end() && *it == time);

  return time;
}

base::Time CookieCreationTimeManager::GetCreationTime(NSHTTPCookie* cookie) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::unordered_map<std::string, base::Time>::iterator it =
      creation_times_.find(GetCookieUniqueID(cookie));
  if (it != creation_times_.end())
    return it->second;

  base::Time native_creation_time = GetCreationTimeFromObject(cookie);
  native_creation_time = MakeUniqueCreationTime(native_creation_time);
  SetCreationTime(cookie, native_creation_time);
  return native_creation_time;
}

void CookieCreationTimeManager::DeleteCreationTime(NSHTTPCookie* cookie) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = creation_times_.find(GetCookieUniqueID(cookie));
  if (it != creation_times_.end()) {
    size_t erased = unique_times_.erase(it->second);
    DCHECK_EQ(1u, erased);
    creation_times_.erase(it);
  }
}

void CookieCreationTimeManager::Clear() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  creation_times_.clear();
  unique_times_.clear();
}

base::WeakPtr<CookieCreationTimeManager>
CookieCreationTimeManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace net
