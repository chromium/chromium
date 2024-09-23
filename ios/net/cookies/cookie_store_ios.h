// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_COOKIE_STORE_IOS_H_
#define IOS_NET_COOKIES_COOKIE_STORE_IOS_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/linked_list.h"
#include "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "ios/net/cookies/cookie_cache.h"
#include "ios/net/cookies/cookie_notification_observer.h"
#import "ios/net/cookies/system_cookie_store.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "url/gurl.h"

@class NSHTTPCookie;
@class NSArray;

namespace net {

class NetLog;
class CookieStoreIOS;

// The CookieStoreIOS is an implementation of CookieStore relying on
// NSHTTPCookieStorage, ensuring that the cookies are consistent between the
// network stack and NSHTTPCookieStorage. CookieStoreIOS is not thread safe.
//
// CookieStoreIOS is created synchronized with the system cookie store -
// changes are written directly to the system cookie store, then propagated to
// the backing store by OnSystemCookiesChanged, which is called by the system
// store once the change to the system store is written back.
class CookieStoreIOS : public net::CookieStore,
                       public CookieNotificationObserver {
 public:
  // Creates an instance of CookieStoreIOS that is generated from the cookies
  // stored in |cookie_storage|. The CookieStoreIOS uses the |cookie_storage|
  // as its default backend and is initially synchronized with it.
  // Apple does not persist the cookies' creation dates in NSHTTPCookieStorage,
  // so callers should not expect these values to be populated.
  CookieStoreIOS(std::unique_ptr<SystemCookieStore> system_store,
                 NetLog* net_log);

  CookieStoreIOS(const CookieStoreIOS&) = delete;
  CookieStoreIOS& operator=(const CookieStoreIOS&) = delete;

  ~CookieStoreIOS() override;

  enum CookiePolicy { ALLOW, BLOCK };

  // Must be called when the state of
  // |NSHTTPCookieStorage sharedHTTPCookieStorage| changes.
  // Affects only those CookieStoreIOS instances that are backed by
  // |NSHTTPCookieStorage sharedHTTPCookieStorage|.
  static void NotifySystemCookiesChanged();

  // Only one cookie store may enable metrics.
  void SetMetricsEnabled();

  // Implementation of the net::CookieStore interface.
  void SetCanonicalCookieAsync(
      std::unique_ptr<CanonicalCookie> cookie,
      const GURL& source_url,
      const net::CookieOptions& options,
      SetCookiesCallback callback,
      std::optional<net::CookieAccessResult> cookie_access_result =
          std::nullopt) override;
  void GetCookieListWithOptionsAsync(
      const GURL& url,
      const net::CookieOptions& options,
      const net::CookiePartitionKeyCollection& cookie_partition_key_collection,
      GetCookieListCallback callback) override;
  void GetAllCookiesAsync(GetAllCookiesCallback callback) override;
  void DeleteCanonicalCookieAsync(const CanonicalCookie& cookie,
                                  DeleteCallback callback) override;
  void DeleteAllCreatedInTimeRangeAsync(
      const net::CookieDeletionInfo::TimeRange& creation_range,
      DeleteCallback callback) override;
  void DeleteAllMatchingInfoAsync(net::CookieDeletionInfo delete_info,
                                  DeleteCallback callback) override;
  void DeleteSessionCookiesAsync(DeleteCallback callback) override;
  void DeleteMatchingCookiesAsync(DeletePredicate predicate,
                                  DeleteCallback callback) override;
  void FlushStore(base::OnceClosure callback) override;
  CookieChangeDispatcher& GetChangeDispatcher() override;
  void SetCookieableSchemes(const std::vector<std::string>& schemes,
                            SetCookieableSchemesCallback callback) override;

 protected:
  CookieStoreIOS(net::CookieMonster::PersistentCookieStore* persistent_store,
                 std::unique_ptr<SystemCookieStore> system_store,
                 NetLog* net_log);

  // These three functions are used for wrapping user-supplied callbacks given
  // to CookieStoreIOS mutator methods. Given a callback, they return a new
  // callback that invokes UpdateCachesFromCookieMonster() to schedule an
  // asynchronous synchronization of the cookie cache and then calls the
  // original callback.
  SetCookiesCallback WrapSetCallback(SetCookiesCallback callback);
  DeleteCallback WrapDeleteCallback(DeleteCallback callback);
  base::OnceClosure WrapClosure(base::OnceClosure callback);

  net::CookieMonster* cookie_monster() { return cookie_monster_.get(); }

  THREAD_CHECKER(thread_checker_);

 private:
  using CookieChangeCallbackList =
      base::RepeatingCallbackList<void(const CookieChangeInfo&)>;

  class Subscription : public base::LinkNode<Subscription>,
                       public CookieChangeSubscription {
   public:
    explicit Subscription(base::CallbackListSubscription subscription);

    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    ~Subscription() override;

    void ResetSubscription();

   private:
    base::CallbackListSubscription subscription_;
  };

  // CookieChangeDispatcher implementation that proxies into IOSCookieStore.
  class CookieChangeDispatcherIOS : public CookieChangeDispatcher {
   public:
    explicit CookieChangeDispatcherIOS(CookieStoreIOS* cookie_store);

    CookieChangeDispatcherIOS(const CookieChangeDispatcherIOS&) = delete;
    CookieChangeDispatcherIOS& operator=(const CookieChangeDispatcherIOS&) =
        delete;

    ~CookieChangeDispatcherIOS() override;

    // net::CookieChangeDispatcher
    [[nodiscard]] std::unique_ptr<CookieChangeSubscription>
    AddCallbackForCookie(
        const GURL& url,
        const std::string& name,
        const std::optional<net::CookiePartitionKey>& cookie_partition_key,
        CookieChangeCallback callback) override;
    [[nodiscard]] std::unique_ptr<CookieChangeSubscription> AddCallbackForUrl(
        const GURL& url,
        const std::optional<net::CookiePartitionKey>& cookie_partition_key,
        CookieChangeCallback callback) override;
    [[nodiscard]] std::unique_ptr<CookieChangeSubscription>
    AddCallbackForAllChanges(CookieChangeCallback callback) override;

   private:
    // Instances of this class are always members of CookieStoreIOS, so
    // |cookie_store| is guaranteed to outlive this instance.
    const raw_ptr<CookieStoreIOS> cookie_store_;
  };

  // Interface only used by CookieChangeDispatcherIOS.
  [[nodiscard]] std::unique_ptr<CookieChangeSubscription> AddCallbackForCookie(
      const GURL& url,
      const std::string& name,
      CookieChangeCallback callback);

  // Returns true if the system cookie store policy is
  // |NSHTTPCookieAcceptPolicyAlways|.
  bool SystemCookiesAllowed();
  // Copies the cookies to the backing CookieMonster.
  virtual void WriteToCookieMonster(NSArray* system_cookies);

  // Inherited CookieNotificationObserver methods.
  void OnSystemCookiesChanged() override;

  void DeleteCookiesMatchingInfoAsync(net::CookieDeletionInfo delete_info,
                                      DeleteCallback callback);

  void DeleteCookiesMatchingPredicateAsync(
      const base::RepeatingCallback<bool(const net::CanonicalCookie&)>&
          predicate,
      DeleteCallback callback);

  // Flush to CookieMonster from |cookies|, and run |callback|.
  void FlushStoreFromCookies(base::OnceClosure callback,
                             NSArray<NSHTTPCookie*>* cookies);

  std::unique_ptr<net::CookieMonster> cookie_monster_;
  std::unique_ptr<SystemCookieStore> system_store_;
  base::CancelableOnceClosure flush_closure_;

  // Cookie notification methods.
  // The cookie cache is updated from both the system store and the
  // CookieStoreIOS' own mutators. Changes when the CookieStoreIOS is
  // synchronized are signalled by the system store; changes when the
  // CookieStoreIOS is not synchronized are signalled by the appropriate
  // mutators on CookieStoreIOS. The cookie cache tracks the system store when
  // the CookieStoreIOS is synchronized and the CookieStore when the
  // CookieStoreIOS is not synchronized.

  // Updates the cookie cache with cookies named |cookie_name| from the current
  // set of |nscookies| that would be sent with a request for |url|.
  // |run_callbacks| Run all callbacks registered for cookie named |name| if
  // CookieCache was changed.
  void UpdateCacheForCookies(const GURL& gurl,
                             const std::string& cookie_name,
                             bool run_callbacks,
                             NSArray<NSHTTPCookie*>* nscookies);

  // Updates the cookie cache with the current set of system cookies named
  // |cookie_name| that would be sent with a request for |url|.
  // |run_callbacks| Run all callbacks registered for cookie named |name| if
  // CookieCache was changed.
  void UpdateCacheForCookieFromSystem(const GURL& gurl,
                                      const std::string& cookie_name,
                                      bool run_callbacks);

  // Runs all callbacks registered for cookies named |name| that would be sent
  // with a request for |url|.
  // All cookies in |cookies| must have the name equal to |name|.
  void RunCallbacksForCookies(const GURL& url,
                              const std::string& name,
                              const std::vector<net::CanonicalCookie>& cookies,
                              net::CookieChangeCause cause);

  // Called by this CookieStoreIOS' internal CookieMonster instance when
  // UpdateCachesFromCookieMonster completes. Updates the cookie cache and runs
  // callbacks if the cache changed.
  void GotCookieListFor(const std::pair<GURL, std::string> key,
                        const net::CookieAccessResultList& cookies,
                        const net::CookieAccessResultList& excluded_cookies);

  // Fetches new values for all (url, name) pairs that have hooks registered,
  // asynchronously invoking callbacks if necessary.
  void UpdateCachesFromCookieMonster();

  // Callback-wrapping:
  // When this CookieStoreIOS object is synchronized with the system store,
  // OnSystemCookiesChanged is responsible for updating the cookie cache (and
  // hence running callbacks).
  //
  // When this CookieStoreIOS object is not synchronized, the various mutator
  // methods (SetCanonicalCookieAsync &c) instead store their state in a
  // CookieMonster object to be written back when the system store synchronizes.
  // To deliver notifications in a timely manner, the mutators have to ensure
  // that hooks get run, but only after the changes have been written back to
  // CookieMonster. To do this, the mutators wrap the user-supplied callback in
  // a callback which schedules an asynchronous task to synchronize the cache
  // and run callbacks, then calls through to the user-specified callback.
  //
  // These three UpdateCachesAfter functions are responsible for scheduling an
  // asynchronous cache update (using UpdateCachesFromCookieMonster()) and
  // calling the provided callback.

  void UpdateCachesAfterSet(SetCookiesCallback callback,
                            net::CookieAccessResult result);
  void UpdateCachesAfterDelete(DeleteCallback callback, uint32_t num_deleted);
  void UpdateCachesAfterClosure(base::OnceClosure callback);

  // Takes an NSArray of NSHTTPCookies as returns a net::CookieList.
  // The returned cookies are ordered by longest path, then earliest
  // creation date.
  net::CookieList CanonicalCookieListFromSystemCookies(NSArray* cookies);

  // Takes an NSArray of NSHTTPCookies as returns a net::CookieAccessResultList.
  // A status of "INCLUDE" is assigned to each cookie.
  // The returned cookies are ordered by longest path, then earliest
  // creation date.
  net::CookieAccessResultList
  CanonicalCookieWithAccessResultListFromSystemCookies(NSArray* cookies);

  // Runs |callback| on CanonicalCookie with status List converted from cookies.
  void RunGetCookieListCallbackOnSystemCookies(GetCookieListCallback callback,
                                               NSArray<NSHTTPCookie*>* cookies);

  // Runs |callback| on CanonicalCookie List converted from cookies.
  void RunGetAllCookiesCallbackOnSystemCookies(GetAllCookiesCallback callback,
                                               NSArray<NSHTTPCookie*>* cookies);

  // Cached values of system cookies. Only cookies which have an observer added
  // with AddCallbackForCookie are kept in this cache.
  std::unique_ptr<CookieCache> cookie_cache_;

  // Callbacks for cookie changes installed by AddCallbackForCookie.
  std::map<std::pair<GURL, std::string>,
           std::unique_ptr<CookieChangeCallbackList>>
      hook_map_;

  base::LinkedList<Subscription> all_subscriptions_;

  CookieChangeDispatcherIOS change_dispatcher_;

  base::WeakPtrFactory<CookieStoreIOS> weak_factory_;
};

}  // namespace net

#endif  // IOS_NET_COOKIES_COOKIE_STORE_IOS_H_
