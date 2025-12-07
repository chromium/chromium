// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_BROWSING_DATA_SYSTEM_COOKIE_STORE_UTIL_H_
#define IOS_WEB_PUBLIC_BROWSING_DATA_SYSTEM_COOKIE_STORE_UTIL_H_

#include <memory>

namespace net {
class SystemCookieStore;
}  // namespace net

namespace web {

class BrowserState;

// On iOS, the system cookie store can only be used on the UI thread, but
// the net::SystemCookieStore object is created on the UI thread and then
// moved to the IO thread where it lives.
//
// CreateSystemCookieStore() returns two objects:
//  - net::SystemCookieStore which is the Chrome object that must be
//    transferred to the IO thread and used there (as on other platforms),
//  - web::SystemCookieStoreHandle which lives on the UI thread, and wraps
//    the system cookie store; once deleted the net::SystemCookieStore will
//    become a null object (i.e. all methods will silently stop working).

class SystemCookieStoreHandle {
 public:
  SystemCookieStoreHandle(const SystemCookieStoreHandle&) = delete;
  SystemCookieStoreHandle& operator=(const SystemCookieStoreHandle&) = delete;

  virtual ~SystemCookieStoreHandle() = default;

 protected:
  SystemCookieStoreHandle() = default;
};

// Returns SystemCookieStore for the given BrowserState.
std::pair<std::unique_ptr<net::SystemCookieStore>,
          std::unique_ptr<SystemCookieStoreHandle>>
CreateSystemCookieStore(BrowserState* browser_state);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_BROWSING_DATA_SYSTEM_COOKIE_STORE_UTIL_H_
