// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by number 42.

#ifndef NET_COOKIES_COOKIE_STORE_H_
#define NET_COOKIES_COOKIE_STORE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "net/base/net_export.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_delegate.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_deletion_info.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_partition_key_collection.h"

class GURL;

namespace net {

class CookieChangeDispatcher;

// An interface for storing and retrieving cookies. Implementations are not
// thread safe, as with most other net classes. All methods must be invoked on
// the network thread, and all callbacks will be called there.
//
// All async functions may either invoke the callback asynchronously, or they
// may be invoked immediately (prior to return of the asynchronous function).
// Destroying the CookieStore will cancel pending async callbacks.
class NET_EXPORT CookieStore {
 public:
  // Callback definitions.
  using GetCookieListCallback =
      base::OnceCallback<void(const CookieAccessResultList& included_cookies,
                              const CookieAccessResultList& excluded_list)>;
  using GetAllCookiesCallback =
      base::OnceCallback<void(const CookieList& cookies)>;
  // |access_semantics_list| is guaranteed to the same length as |cookies|.
  using GetAllCookiesWithAccessSemanticsCallback = base::OnceCallback<void(
      const CookieList& cookies,
      const std::vector<CookieAccessSemantics>& access_semantics_list)>;
  using SetCookiesCallback =
      base::OnceCallback<void(CookieAccessResult access_result)>;
  using DeleteCallback = base::OnceCallback<void(uint32_t num_deleted)>;
  using DeletePredicate =
      base::RepeatingCallback<bool(const CanonicalCookie& cookie)>;
  using SetCookieableSchemesCallback = base::OnceCallback<void(bool success)>;

  CookieStore();
  virtual ~CookieStore();

  // Set the cookie on the cookie store.  |cookie.IsCanonical()| must
  // be true.  |source_url| denotes the url of the resource setting this.
  //
  // |options| is used to determine the context the operation is run in, and
  // which cookies it can alter (e.g. http only, or same site).
  //
  // The current time will be used in place of a null creation time.
  //
  // |cookie_access_result| is an optional input status, to allow for status
  // chaining from callers. It helps callers provide the status of a
  // canonical cookie that may have warnings associated with it.
  virtual void SetCanonicalCookieAsync(
      std::unique_ptr<CanonicalCookie> cookie,
      const GURL& source_url,
      const CookieOptions& options,
      SetCookiesCallback callback,
      std::optional<CookieAccessResult> cookie_access_result =
          std::nullopt) = 0;

  // Obtains a CookieList for the given |url| and |options|. The returned
  // cookies are passed into |callback|, ordered by longest path, then earliest
  // creation date.
  // To get all the cookies for a URL, use this method with an all-inclusive
  // |options|.
  // If |cookie_partition_key_collection| is not empty, then this function will
  // return the partitioned cookies for that URL whose partition keys are in the
  // cookie_partition_key_collection *in addition to* the unpartitioned cookies
  // for that URL.
  virtual void GetCookieListWithOptionsAsync(
      const GURL& url,
      const CookieOptions& options,
      const CookiePartitionKeyCollection& cookie_partition_key_collection,
      GetCookieListCallback callback) = 0;

  // Returns all the cookies, for use in management UI, etc. This does not mark
  // the cookies as having been accessed. The returned cookies are ordered by
  // longest path, then by earliest creation date.
  virtual void GetAllCookiesAsync(GetAllCookiesCallback callback) = 0;

  // Returns all the cookies, for use in management UI, etc. This does not mark
  // the cookies as having been accessed. The returned cookies are ordered by
  // longest path, then by earliest creation date.
  // Additionally returns a vector of CookieAccessSemantics values for the
  // returned cookies, which will be the same length as the vector of returned
  // cookies. This vector will either contain all CookieAccessSemantics::UNKNOWN
  // (if the default implementation is used), or each entry in the
  // vector of CookieAccessSemantics will indicate the access semantics
  // applicable to the cookie at the same index in the returned CookieList.
  virtual void GetAllCookiesWithAccessSemanticsAsync(
      GetAllCookiesWithAccessSemanticsCallback callback);

  // Deletes one specific cookie. |cookie| must have been returned by a previous
  // query on this CookieStore. Invokes |callback| with 1 if a cookie was
  // deleted, 0 otherwise.
  virtual void DeleteCanonicalCookieAsync(const CanonicalCookie& cookie,
                                          DeleteCallback callback) = 0;

  // Deletes all of the cookies that have a creation_date matching
  // |creation_range|. See CookieDeletionInfo::TimeRange::Matches().
  // Calls |callback| with the number of cookies deleted.
  virtual void DeleteAllCreatedInTimeRangeAsync(
      const CookieDeletionInfo::TimeRange& creation_range,
      DeleteCallback callback) = 0;

  // Deletes all of the cookies matching |delete_info|. This includes all
  // http_only and secure cookies. Avoid deleting cookies that could leave
  // websites with a partial set of visible cookies.
  // Calls |callback| with the number of cookies deleted.
  virtual void DeleteAllMatchingInfoAsync(CookieDeletionInfo delete_info,
                                          DeleteCallback callback) = 0;

  // Deletes all cookies without expiration data.
  virtual void DeleteSessionCookiesAsync(DeleteCallback callback) = 0;

  // Deletes all cookies where |predicate| returns true.
  // Calls |callback| with the number of cookies deleted.
  virtual void DeleteMatchingCookiesAsync(DeletePredicate predicate,
                                          DeleteCallback callback) = 0;

  // Deletes all cookies in the store.
  void DeleteAllAsync(DeleteCallback callback);

  // Flush the backing store (if any) to disk and post the given callback when
  // done.
  virtual void FlushStore(base::OnceClosure callback) = 0;

  // Protects session cookies from deletion on shutdown, if the underlying
  // CookieStore implemention is currently configured to store them to disk.
  // Otherwise, does nothing.
  virtual void SetForceKeepSessionState() {}

  // The interface used to observe changes to this CookieStore's contents.
  virtual CookieChangeDispatcher& GetChangeDispatcher() = 0;

  // Resets the list of cookieable schemes to the supplied schemes. Does nothing
  // (and returns false) if called after first use of the instance (i.e. after
  // the instance initialization process). Otherwise, this returns true to
  // indicate success. CookieStores which do not support modifying cookieable
  // schemes will always return false.
  virtual void SetCookieableSchemes(const std::vector<std::string>& schemes,
                                    SetCookieableSchemesCallback callback) = 0;

  // Transfer ownership of a CookieAccessDelegate.
  void SetCookieAccessDelegate(std::unique_ptr<CookieAccessDelegate> delegate);

  // This may be null if no delegate has been set yet, or the delegate has been
  // reset to null.
  const CookieAccessDelegate* cookie_access_delegate() const {
    return cookie_access_delegate_.get();
  }

  // Returns a boolean indicating whether the cookie `site` has any cookie
  // in another partition other than the `cookie_partition_key` supplied.
  // Will return nullopt if cookies have not finished loading.
  // If the partition key is null, the method assumes it is because partitioned
  // cookies are disabled.
  virtual std::optional<bool> SiteHasCookieInOtherPartition(
      const net::SchemefulSite& site,
      const std::optional<CookiePartitionKey>& cookie_partition_key) const;

 private:
  // Used to determine whether a particular cookie should be subject to legacy
  // or non-legacy access semantics.
  std::unique_ptr<CookieAccessDelegate> cookie_access_delegate_;
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_STORE_H_
