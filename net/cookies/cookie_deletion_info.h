// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_DELETION_INFO_H_
#define NET_COOKIES_COOKIE_DELETION_INFO_H_

#include <optional>
#include <set>
#include <string>

#include "base/time/time.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key_collection.h"

namespace net {

// Used to specify which cookies to delete. All members are ANDed together.
struct NET_EXPORT CookieDeletionInfo {
  // TODO(cmumford): Combine with
  // network::mojom::CookieDeletionSessionControl.
  enum SessionControl {
    IGNORE_CONTROL,
    SESSION_COOKIES,
    PERSISTENT_COOKIES,
  };

  // Define a range of time from [start, end) where start is inclusive and end
  // is exclusive. There is a special case where |start| == |end| (matching a
  // single time) where |end| is inclusive. This special case is for iOS that
  // will be removed in the future.
  //
  // TODO(crbug.com/40570811): Delete the start=end special case.
  class NET_EXPORT TimeRange {
   public:
    // Default constructor matches any non-null time.
    TimeRange();
    TimeRange(const TimeRange& other);
    TimeRange(base::Time start, base::Time end);
    TimeRange& operator=(const TimeRange& rhs);

    // Is |time| within this time range?
    //
    // Will return true if:
    //
    //   |start_| <= |time| < |end_|
    //
    // If |start_| is null then the range is unbounded on the lower range.
    // If |end_| is null then the range is unbounded on the upper range.
    //
    // Note 1: |time| cannot be null.
    // Note 2: If |start_| == |end_| then end_ is inclusive.
    //
    bool Contains(const base::Time& time) const;

    // Set the range start time. Set to null (i.e. Time()) to indicated an
    // unbounded lower range.
    void SetStart(base::Time value);

    // Set the range end time. Set to null (i.e. Time()) to indicated an
    // unbounded upper range.
    void SetEnd(base::Time value);

    // Return the start time.
    base::Time start() const { return start_; }

    // Return the end time.
    base::Time end() const { return end_; }

   private:
    // The inclusive start time of this range.
    base::Time start_;
    // The exclusive end time of this range.
    base::Time end_;
  };

  CookieDeletionInfo();
  CookieDeletionInfo(CookieDeletionInfo&& other);
  CookieDeletionInfo(const CookieDeletionInfo& other);
  CookieDeletionInfo(base::Time start_time, base::Time end_time);
  ~CookieDeletionInfo();

  CookieDeletionInfo& operator=(CookieDeletionInfo&& rhs);
  CookieDeletionInfo& operator=(const CookieDeletionInfo& rhs);

  // Return true if |cookie| matches all members of this instance. All members
  // are ANDed together. For example: if the |cookie| creation date is within
  // |creation_range| AND the |cookie| name is equal to |name|, etc. then true
  // will be returned. If not false.
  //
  // |params.access_semantics| is the access semantics mode of the cookie at the
  // time of the attempted match. This is used to determine whether the cookie
  // matches a particular URL based on effective SameSite mode. (But the value
  // should not matter because the CookieOptions used for this check includes
  // all cookies for a URL regardless of SameSite).
  //
  // |params.delegate_treats_url_as_trustworthy| should be set to true if |url|
  // was granted access to secure cookies by the CookieAccessDelegate.
  //
  // All members are used. See comments above other members for specifics
  // about how checking is done for that value.
  bool Matches(const CanonicalCookie& cookie,
               const CookieAccessParams& params) const;

  // See comment above for TimeRange::Contains() for more info.
  TimeRange creation_range;

  // By default ignore session type and delete both session and persistent
  // cookies.
  SessionControl session_control = SessionControl::IGNORE_CONTROL;

  // If has a value then cookie.Host() must equal |host|.
  std::optional<std::string> host;

  // If has a value then cookie.Name() must equal |name|.
  std::optional<std::string> name;

  // If has a value then will match if the cookie being evaluated would be
  // included for a request of |url|.
  std::optional<GURL> url;

  // If has a value then any cookie with a domain/ip contained in this set
  // will be deleted (assuming other fields match).
  // Domains must not have a leading period. e.g "example.com" and not
  // ".example.com".
  //
  // Note: |domains_and_ips_to_ignore| takes precedence. For example if this
  // has a value of ["A", "B"] and |domains_and_ips_to_ignore| is ["B", "C"]
  // then only "A" will be deleted.
  std::optional<std::set<std::string>> domains_and_ips_to_delete;

  // If has a value then any cookie with a domain/ip contained in this set
  // will be ignored (and not deleted).
  // Domains must not have a leading period. e.g "example.com" and not
  // ".example.com".
  //
  // See precedence note above.
  std::optional<std::set<std::string>> domains_and_ips_to_ignore;

  // Used only for testing purposes.
  std::optional<std::string> value_for_testing;

  // Cookie partition collection. Partitioned cookies are not deleted if their
  // partition key is not in the collection. By default, it clears cookies in
  // all partitions.
  CookiePartitionKeyCollection cookie_partition_key_collection =
      CookiePartitionKeyCollection::ContainsAll();

  // If true, third-party cookie blocking applies to the context that triggered
  // the deletion. In this case, we should only delete partitioned cookies.
  bool partitioned_state_only = false;
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_DELETION_INFO_H_
