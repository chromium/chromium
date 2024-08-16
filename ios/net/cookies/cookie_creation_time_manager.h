// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_COOKIE_CREATION_TIME_MANAGER_H_
#define IOS_NET_COOKIES_COOKIE_CREATION_TIME_MANAGER_H_

#include <set>
#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

@class NSHTTPCookie;

namespace net {

// The CookieCreationTimeManager allows to get and set the creation time of a
// NSHTTPCookie.
// All the methods of CookieCreationTimeManager must be called on the IO thread,
// except its constructor that can be called from any thread.
// Creation time data is stored either in the cookie properties (when the cookie
// is created by the system) or in |creation_times_| (when the cookie is created
// by CookieStoreIOS). When both are available, |creation_times_| is used,
// because the cookie property only has a one second precision.
class CookieCreationTimeManager {
 public:
  CookieCreationTimeManager();
  ~CookieCreationTimeManager();
  // Sets the creation time for |cookie|.
  // |creation_time| must be unique (not used by another cookie).
  void SetCreationTime(NSHTTPCookie* cookie, const base::Time& creation_time);
  // Creates a unique creation time (to be used in SetCreationTime()) that is
  // as close as possible to |creation_time|.
  base::Time MakeUniqueCreationTime(const base::Time& creation_time);
  // Gets the creation time for |cookie|.
  base::Time GetCreationTime(NSHTTPCookie* cookie);
  // Deletes the creation time for |cookie|.
  void DeleteCreationTime(NSHTTPCookie* cookie);
  // Clears all the creation times.
  void Clear();
  // Gets base::WeakPtr to the object to be used in sorting.
  base::WeakPtr<CookieCreationTimeManager> GetWeakPtr();

 private:
  std::unordered_map<std::string, base::Time> creation_times_;
  std::set<base::Time> unique_times_;
  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<CookieCreationTimeManager> weak_factory_;
};

}  // namespace net

#endif  // IOS_NET_COOKIES_COOKIE_CREATION_TIME_MANAGER_H_
