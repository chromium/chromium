// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_RESTRICTED_COOKIE_MANAGER_H_
#define SERVICES_NETWORK_RESTRICTED_COOKIE_MANAGER_H_

#include <set>
#include <string>
#include <tuple>

#include "base/component_export.h"
#include "base/containers/linked_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_store.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
class CookieStore;
class SiteForCookies;
}  // namespace net

namespace network {

struct CookieWithAccessResultComparer {
  bool operator()(
      const net::CookieWithAccessResult& cookie_with_access_result1,
      const net::CookieWithAccessResult& cookie_with_access_result2) const;
};

using CookieAccesses =
    std::set<net::CookieWithAccessResult, CookieWithAccessResultComparer>;
using CookieAccessesByURLAndSite =
    std::map<std::pair<GURL, net::SiteForCookies>,
             std::unique_ptr<CookieAccesses>>;

class CookieSettings;

// RestrictedCookieManager implementation.
//
// Instances of this class must be created and used on the sequence that hosts
// the CookieStore passed to the constructor.
class COMPONENT_EXPORT(NETWORK_SERVICE) RestrictedCookieManager
    : public mojom::RestrictedCookieManager {
 public:
  // All the pointers passed to the constructor are expected to point to
  // objects that will outlive `this`.
  //
  // `origin` represents the domain for which the RestrictedCookieManager can
  // access cookies. It could either be a frame origin when `role` is
  // RestrictedCookieManagerRole::SCRIPT (a script scoped to a particular
  // document's frame)), or a request origin when `role` is
  // RestrictedCookieManagerRole::NETWORK (a network request).
  //
  // `isolation_info` must be fully populated, its `frame_origin` field should
  // not be used for cookie access decisions, but should be the same as `origin`
  // if the `role` is mojom::RestrictedCookieManagerRole::SCRIPT.
  //
  // `first_party_set_metadata` should have been previously computed by
  // `ComputeFirstPartySetMetadata` using the same `origin`, `cookie_store` and
  // `isolation_info` as were passed in here.
  RestrictedCookieManager(
      mojom::RestrictedCookieManagerRole role,
      net::CookieStore* cookie_store,
      const CookieSettings& cookie_settings,
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info,
      mojo::PendingRemote<mojom::CookieAccessObserver> cookie_observer,
      net::FirstPartySetMetadata first_party_set_metadata);

  RestrictedCookieManager(const RestrictedCookieManager&) = delete;
  RestrictedCookieManager& operator=(const RestrictedCookieManager&) = delete;

  ~RestrictedCookieManager() override;

  void OverrideOriginForTesting(const url::Origin& new_origin) {
    origin_ = new_origin;
  }

  // This spins the event loop, since the cookie partition key may be computed
  // asynchronously.
  void OverrideIsolationInfoForTesting(
      const net::IsolationInfo& new_isolation_info);

  const CookieSettings& cookie_settings() const { return *cookie_settings_; }

  void GetAllForUrl(const GURL& url,
                    const net::SiteForCookies& site_for_cookies,
                    const url::Origin& top_frame_origin,
                    mojom::CookieManagerGetOptionsPtr options,
                    GetAllForUrlCallback callback) override;

  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& url,
                          const net::SiteForCookies& site_for_cookies,
                          const url::Origin& top_frame_origin,
                          net::CookieInclusionStatus status,
                          SetCanonicalCookieCallback callback) override;

  void AddChangeListener(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      mojo::PendingRemote<mojom::CookieChangeListener> listener,
      AddChangeListenerCallback callback) override;

  void SetCookieFromString(const GURL& url,
                           const net::SiteForCookies& site_for_cookies,
                           const url::Origin& top_frame_origin,
                           const std::string& cookie,
                           SetCookieFromStringCallback callback) override;

  void GetCookiesString(const GURL& url,
                        const net::SiteForCookies& site_for_cookies,
                        const url::Origin& top_frame_origin,
                        GetCookiesStringCallback callback) override;
  void CookiesEnabledFor(const GURL& url,
                         const net::SiteForCookies& site_for_cookies,
                         const url::Origin& top_frame_origin,
                         CookiesEnabledForCallback callback) override;

  // Computes the First-Party Set metadata corresponding to the given `origin`,
  // `cookie_store`, and `isolation_info`.
  //
  // May invoke `callback` either synchronously or asynchronously.
  static void ComputeFirstPartySetMetadata(
      const url::Origin& origin,
      const net::CookieStore* cookie_store,
      const net::IsolationInfo& isolation_info,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback);

 private:
  // The state associated with a CookieChangeListener.
  class Listener;

  // Returns true if the RCM instance can read and/or set partitioned cookies.
  bool IsPartitionedCookiesEnabled() const;

  // Feeds a net::CookieList to a GetAllForUrl() callback.
  void CookieListToGetAllForUrlCallback(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      const net::CookieOptions& net_options,
      mojom::CookieManagerGetOptionsPtr options,
      GetAllForUrlCallback callback,
      const net::CookieAccessResultList& cookie_list,
      const net::CookieAccessResultList& excluded_cookies);

  // Reports the result of setting the cookie to |network_context_client_|, and
  // invokes the user callback.
  void SetCanonicalCookieResult(const GURL& url,
                                const net::SiteForCookies& site_for_cookies,
                                const net::CanonicalCookie& cookie,
                                const net::CookieOptions& net_options,
                                SetCanonicalCookieCallback user_callback,
                                net::CookieAccessResult access_result);

  // Called when the Mojo pipe associated with a listener is closed.
  void RemoveChangeListener(Listener* listener);

  // Ensures that this instance may access the cookies for a given URL.
  //
  // Returns true if the access should be allowed, or false if it should be
  // blocked.
  //
  // |cookie_being_set| should be non-nullptr if setting a cookie, and should be
  // nullptr otherwise (getting cookies, subscribing to cookie changes).
  //
  // If the access would not be allowed, this helper calls
  // mojo::ReportBadMessage(), which closes the pipe.
  bool ValidateAccessToCookiesAt(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      const net::CanonicalCookie* cookie_being_set = nullptr);

  const net::SiteForCookies& BoundSiteForCookies() const {
    return isolation_info_.site_for_cookies();
  }

  const url::Origin& BoundTopFrameOrigin() const {
    return isolation_info_.top_frame_origin().value();
  }

  CookieAccesses* GetCookieAccessesForURLAndSite(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies);

  // Returns true if the RCM should skip sending a cookie access notification
  // to the |cookie_observer_| for the cookie in |cookie_item|.
  bool SkipAccessNotificationForCookieItem(
      CookieAccesses* cookie_accesses,
      const net::CookieWithAccessResult& cookie_item);

  // Called while overriding the cookie_partition_key during testing.
  void OnGotFirstPartySetMetadataForTesting(
      base::OnceClosure done_closure,
      net::FirstPartySetMetadata first_party_set_metadata);

  // Computes the CookieSettingOverrides to be used by this instance.
  net::CookieSettingOverrides GetCookieSettingOverrides() const;

  const mojom::RestrictedCookieManagerRole role_;
  const raw_ptr<net::CookieStore> cookie_store_;
  const raw_ref<const CookieSettings> cookie_settings_;

  url::Origin origin_;

  // Holds the browser-provided site_for_cookies and top_frame_origin to which
  // this RestrictedCookieManager is bound. (The frame_origin field is not used
  // directly, but must match the `origin_` if the RCM role is SCRIPT.)
  net::IsolationInfo isolation_info_;

  mojo::Remote<mojom::CookieAccessObserver> cookie_observer_;

  base::LinkedList<Listener> listeners_;

  SEQUENCE_CHECKER(sequence_checker_);

  // The First-Party Set metadata for the context this RestrictedCookieManager
  // is associated with.
  net::FirstPartySetMetadata first_party_set_metadata_;

  // Cookie partition key that the instance of RestrictedCookieManager will have
  // access to. Must be set only in the constructor or in *ForTesting methods.
  absl::optional<net::CookiePartitionKey> cookie_partition_key_;
  // CookiePartitionKeyCollection that is either empty if
  // `cookie_partition_key_` is nullopt. If `cookie_partition_key_` is not null,
  // the key collection contains its value. Must be kept in sync with
  // `cookie_partition_key_`.
  net::CookiePartitionKeyCollection cookie_partition_key_collection_;

  // Contains a mapping of url/site -> recent cookie updates for duplicate
  // update filtering.
  CookieAccessesByURLAndSite recent_cookie_accesses_;

  bool same_party_attribute_enabled_;

  base::WeakPtrFactory<RestrictedCookieManager> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_RESTRICTED_COOKIE_MANAGER_H_
