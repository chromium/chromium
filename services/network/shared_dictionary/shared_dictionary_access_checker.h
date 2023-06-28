// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_ACCESS_CHECKER_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_ACCESS_CHECKER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/shared_dictionary_access_observer.mojom.h"

class GURL;

namespace net {
class IsolationInfo;
class SiteForCookies;
}  // namespace net

namespace network {

class NetworkContext;

// This class determines whether a network transaction is allowed to use a
// shared dictionary, and asynchronously reports the usage to the browser
// process. The browser process then shows a UI to the user that indicates that
// site data was used.
class COMPONENT_EXPORT(NETWORK_SERVICE) SharedDictionaryAccessChecker final {
 public:
  SharedDictionaryAccessChecker(
      NetworkContext& context,
      mojo::PendingRemote<mojom::SharedDictionaryAccessObserver>
          shared_dictionary_observer_remote);
  SharedDictionaryAccessChecker(
      NetworkContext& context,
      mojom::SharedDictionaryAccessObserver* shared_dictionary_observer);
  ~SharedDictionaryAccessChecker();

  SharedDictionaryAccessChecker(const SharedDictionaryAccessChecker&) = delete;
  SharedDictionaryAccessChecker& operator=(
      const SharedDictionaryAccessChecker&) = delete;

  bool CheckAllowedToWriteAndReport(const GURL& dictionary_url,
                                    const net::SiteForCookies& site_for_cookies,
                                    const net::IsolationInfo& isolation_info);
  bool CheckAllowedToReadAndReport(const GURL& target_resource_url,
                                   const net::SiteForCookies& site_for_cookies,
                                   const net::IsolationInfo& isolation_info);

 private:
  bool IsAllowedToUseSharedDictionary(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const net::IsolationInfo& isolation_info);

  const raw_ref<NetworkContext> context_;
  mojo::Remote<mojom::SharedDictionaryAccessObserver>
      shared_dictionary_observer_remote_;
  raw_ptr<mojom::SharedDictionaryAccessObserver> shared_dictionary_observer_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_ACCESS_CHECKER_H_
