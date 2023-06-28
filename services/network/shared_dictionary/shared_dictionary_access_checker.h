// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_ACCESS_CHECKER_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_ACCESS_CHECKER_H_

#include "base/memory/raw_ref.h"

class GURL;

namespace net {
class IsolationInfo;
class SiteForCookies;
}  // namespace net

namespace network {

class NetworkContext;

// This class is used to determine whether a network transaction is allowed to
// use the dictionary.
class SharedDictionaryAccessChecker {
 public:
  SharedDictionaryAccessChecker(NetworkContext& context);
  virtual ~SharedDictionaryAccessChecker();

  SharedDictionaryAccessChecker(const SharedDictionaryAccessChecker&) = delete;
  SharedDictionaryAccessChecker& operator=(
      const SharedDictionaryAccessChecker&) = delete;

  bool IsAllowedToWrite(const GURL& dictionary_url,
                        const net::SiteForCookies& site_for_cookies,
                        const net::IsolationInfo& isolation_info);
  bool IsAllowedToRead(const GURL& target_resource_url,
                       const net::SiteForCookies& site_for_cookies,
                       const net::IsolationInfo& isolation_info);

 private:
  bool IsAllowedToUseSharedDictionary(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const net::IsolationInfo& isolation_info);

  const raw_ref<NetworkContext> context_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_ACCESS_CHECKER_H_
