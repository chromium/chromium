// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_SYSTEM_COOKIE_STORE_H_
#define IOS_NET_COOKIES_SYSTEM_COOKIE_STORE_H_

#import <Foundation/Foundation.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

class GURL;

namespace net {

class CookieCreationTimeManager;

// SystemCookieStore is an abstract class that should be implemented for every
// type of system store (WKHTTPCookieStore, NSHTTPCookieStorage, ..), its main
// purpose is to interact with the system cookie store, and let the caller use
// it directly without caring about the type of the underlying cookie store.
// The class methods should only be called from IO Thread.
class SystemCookieStore {
 public:
  // Callback definitions.
  typedef base::OnceClosure SystemCookieCallback;
  typedef base::OnceCallback<void(NSArray<NSHTTPCookie*>*)>
      SystemCookieCallbackForCookies;

  SystemCookieStore();
  virtual ~SystemCookieStore();

  // Calls |callback| on all cookies for a specific |url| in the internal
  // cookie store.
  // If CookieCreationTimeManager was provided in the constructor, sort cookies
  // as per RFC6265 before calling the |callback|.
  virtual void GetCookiesForURLAsync(
      const GURL& url,
      SystemCookieCallbackForCookies callback) = 0;

  // Calls |callback| on all cookies in the internal cookie store.
  // If CookieCreationTimeManager was provided in the constructor, sort cookies
  // as per RFC6265 before calling the |callback|.
  virtual void GetAllCookiesAsync(SystemCookieCallbackForCookies callback) = 0;

  // Deletes a specific cookie from the internal cookie store, and call
  // |callback| after it's deleted.
  virtual void DeleteCookieAsync(NSHTTPCookie* cookie,
                                 SystemCookieCallback callback) = 0;

  // Sets a specific cookie to the internal cookie store, sets the cookie
  // creation time |optional_creation_time| or to the current time if
  // |optional_creation_time| is nil, then calls |callback| after it's set.
  virtual void SetCookieAsync(NSHTTPCookie* cookie,
                              const base::Time* optional_creation_time,
                              SystemCookieCallback callback) = 0;

  // Same as SetCookieAsync but uses actual time of setting the cookie.
  void SetCookieAsync(NSHTTPCookie* cookie, SystemCookieCallback callback);

  // Deletes all cookies from the internal http cookie store, and calls
  // |callback| all cookies are deleted.
  virtual void ClearStoreAsync(SystemCookieCallback callback) = 0;

  // Returns the Cookie Accept policy for the internal cookie store.
  virtual NSHTTPCookieAcceptPolicy GetCookieAcceptPolicy() = 0;

  // Returns the creation time of a specific cookie
  base::Time GetCookieCreationTime(NSHTTPCookie* cookie);

  // Return WeakPtr of this object.
  base::WeakPtr<SystemCookieStore> GetWeakPtr();

 protected:
  // Compares cookies based on the path lengths and the creation times provided
  // by a non null creation time manager |context|, as per RFC6265.
  static NSInteger CompareCookies(NSHTTPCookie* cookie_a,
                                  NSHTTPCookie* cookie_b,
                                  void* context);

  // Internal cookie stores doesn't store creation time. This object is used
  // to keep track of the creation time of cookies, this is required for
  // conversion between SystemCookie and Chromium CookieMonster.
  // TODO(crbug.com/40568476): Move this to be private.
  std::unique_ptr<CookieCreationTimeManager> creation_time_manager_;

 private:
  // Weak Ptr factory.
  base::WeakPtrFactory<SystemCookieStore> weak_factory_;
};

}  // namespace net

#endif  // IOS_NET_COOKIES_SYSTEM_COOKIE_STORE_H_
