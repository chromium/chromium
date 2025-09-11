// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BTM_BTM_UTILS_H_
#define CONTENT_BROWSER_BTM_BTM_UTILS_H_

#include <optional>
#include <ostream>
#include <string_view>

#include "base/files/file_path.h"
#include "base/strings/cstring_view.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/btm_redirect_info.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "url/gurl.h"

namespace base {
class TimeDelta;
}

namespace url {
class Origin;
}

namespace content {

class BrowserContext;

// For use in tests/debugging.
CONTENT_EXPORT base::cstring_view BtmCookieModeToString(BtmCookieMode mode);
CONTENT_EXPORT base::cstring_view BtmRedirectTypeToString(BtmRedirectType type);
CONTENT_EXPORT base::cstring_view BtmDataAccessTypeToString(
    BtmDataAccessType type);

// A single cookie-accessing operation (either read or write). Not to be
// confused with BtmDataAccessType, which can also represent no access or both
// read+write.
using CookieOperation = network::mojom::CookieAccessDetails::Type;

// The filename for the DIPS database.
const base::FilePath::CharType kBtmFilename[] = FILE_PATH_LITERAL("DIPS");

// The FilePath for the ON-DISK BtmDatabase associated with a BrowserContext,
// if one exists.
// NOTE: This returns the same value regardless of if there is actually a
// persisted BtmDatabase for the BrowserContext or not.
CONTENT_EXPORT base::FilePath GetBtmFilePath(BrowserContext* context);

inline BtmDataAccessType ToBtmDataAccessType(CookieOperation op) {
  return (op == CookieOperation::kChange ? BtmDataAccessType::kWrite
                                         : BtmDataAccessType::kRead);
}
CONTENT_EXPORT std::ostream& operator<<(std::ostream& os,
                                        BtmDataAccessType access_type);

constexpr BtmDataAccessType operator|(BtmDataAccessType lhs,
                                      BtmDataAccessType rhs) {
  return static_cast<BtmDataAccessType>(static_cast<int>(lhs) |
                                        static_cast<int>(rhs));
}
inline BtmDataAccessType& operator|=(BtmDataAccessType& lhs,
                                     BtmDataAccessType rhs) {
  return (lhs = lhs | rhs);
}

BtmCookieMode GetBtmCookieMode(bool is_otr);
std::string_view GetHistogramSuffix(BtmCookieMode mode);
std::ostream& operator<<(std::ostream& os, BtmCookieMode mode);

// BtmEventRemovalType:
// NOTE: We use this type as a bitfield don't change existing values other than
// kAll, which should be updated to include any new fields.
enum class BtmEventRemovalType {
  kNone = 0,
  kHistory = 1 << 0,
  kStorage = 1 << 1,
  // kAll is intended to cover all the above fields.
  kAll = kHistory | kStorage
};

constexpr BtmEventRemovalType operator|(BtmEventRemovalType lhs,
                                        BtmEventRemovalType rhs) {
  return static_cast<BtmEventRemovalType>(static_cast<int>(lhs) |
                                          static_cast<int>(rhs));
}

constexpr BtmEventRemovalType operator&(BtmEventRemovalType lhs,
                                        BtmEventRemovalType rhs) {
  return static_cast<BtmEventRemovalType>(static_cast<int>(lhs) &
                                          static_cast<int>(rhs));
}

constexpr BtmEventRemovalType& operator|=(BtmEventRemovalType& lhs,
                                          BtmEventRemovalType rhs) {
  return lhs = lhs | rhs;
}

constexpr BtmEventRemovalType& operator&=(BtmEventRemovalType& lhs,
                                          BtmEventRemovalType rhs) {
  return lhs = lhs & rhs;
}

std::string_view GetHistogramPiece(BtmRedirectType type);
CONTENT_EXPORT std::ostream& operator<<(std::ostream& os, BtmRedirectType type);

using TimestampRange = std::optional<std::pair<base::Time, base::Time>>;
// Expand the range to include `time` if necessary. Returns true iff the range
// was modified.
CONTENT_EXPORT bool UpdateTimestampRange(TimestampRange& range,
                                         base::Time time);
// Checks that `this` range is either null or falls within `other`.
CONTENT_EXPORT bool IsNullOrWithin(const TimestampRange& inner,
                                   const TimestampRange& outer);

std::ostream& operator<<(std::ostream& os, TimestampRange type);

// Values for a site in the `bounces` table.
struct StateValue {
  TimestampRange user_activation_times;
  TimestampRange bounce_times;
  TimestampRange web_authn_assertion_times;
};

// Values for a 1P/3P site pair in the `popups` table.
struct PopupsStateValue {
  uint64_t access_id;
  base::Time last_popup_time;
  bool is_current_interaction;
  bool is_authentication_interaction;
};

struct PopupWithTime {
  std::string opener_site;
  std::string popup_site;
  base::Time last_popup_time;
};

inline bool operator==(const StateValue& lhs, const StateValue& rhs) {
  return std::tie(lhs.user_activation_times, lhs.bounce_times,
                  lhs.web_authn_assertion_times) ==
         std::tie(rhs.user_activation_times, rhs.bounce_times,
                  rhs.web_authn_assertion_times);
}

// Return the number of seconds in `delta`, clamped to [0, 10].
// i.e. 11 linearly-sized buckets.
CONTENT_EXPORT int64_t BucketizeBtmBounceDelay(base::TimeDelta delta);

// Returns an opaque value representing the "privacy boundary" that the URL
// belongs to. Currently returns eTLD+1, but this is an implementation detail
// and may change.
CONTENT_EXPORT std::string GetSiteForBtm(const GURL& url);
CONTENT_EXPORT std::string GetSiteForBtm(const url::Origin& origin);

// Returns true iff `web_contents` contains an iframe whose committed URL
// belongs to the same site as `url`.
bool HasSameSiteIframe(WebContents* web_contents, const GURL& url);

CONTENT_EXPORT bool HasCHIPS(
    const net::CookieAccessResultList& cookie_access_result_list);

// Returns `True` iff the `navigation_handle` represents a navigation
// happening in an iframe of the primary frame tree.
inline bool IsInPrimaryPageIFrame(NavigationHandle& navigation_handle) {
  return navigation_handle.GetParentFrame()
             ? navigation_handle.GetParentFrame()->GetPage().IsPrimary()
             : false;
}

// Returns `True` iff both urls return a similar outcome off of
// `GetSiteForBtm()`.
inline bool IsSameSiteForBtm(const GURL& url1, const GURL& url2) {
  return GetSiteForBtm(url1) == GetSiteForBtm(url2);
}

// Returns `True` iff the `navigation_handle` represents a navigation happening
// in any frame of the primary page.
// NOTE: This does not include fenced frames.
inline bool IsInPrimaryPage(NavigationHandle& navigation_handle) {
  return navigation_handle.GetParentFrame()
             ? navigation_handle.GetParentFrame()->GetPage().IsPrimary()
             : navigation_handle.IsInPrimaryMainFrame();
}

// Returns `True` iff the 'rfh' exists and represents a frame in the primary
// page.
inline bool IsInPrimaryPage(RenderFrameHost& rfh) {
  return rfh.GetPage().IsPrimary();
}

// Returns the last committed or the to be committed url of the main frame of
// the page containing the `navigation_handle`.
inline const GURL& GetFirstPartyURL(NavigationHandle& navigation_handle) {
  return navigation_handle.GetParentFrame()
             ? navigation_handle.GetParentFrame()
                   ->GetMainFrame()
                   ->GetLastCommittedURL()
             : navigation_handle.GetURL();
}

// Returns an optional last committed url of the main frame of the page
// containing the `rfh`.
inline const GURL& GetFirstPartyURL(RenderFrameHost& rfh) {
  return rfh.GetMainFrame()->GetLastCommittedURL();
}

// The amount of time since a page last received user activation before a
// subsequent user activation event may be recorded to DIPS Storage for the
// same page.
inline constexpr base::TimeDelta kBtmTimestampUpdateInterval = base::Minutes(1);

[[nodiscard]] CONTENT_EXPORT bool UpdateTimestamp(
    std::optional<base::Time>& last_time,
    base::Time now);

// BtmInteractionType is used in UKM to record the way the user interacted with
// the site. It should match CookieHeuristicInteractionType in
// tools/metrics/ukm/ukm.xml
enum class BtmInteractionType {
  Authentication = 0,
  UserActivation = 1,
  NoInteraction = 2,
};

enum class BtmRecordedEvent {
  kUserActivation,
  kWebAuthnAssertion,
};

// BtmRedirectCategory is basically the cross-product of BtmDataAccessType and
// a boolean value indicating site engagement. It's used in UMA enum histograms.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BtmRedirectCategory {
  kNoCookies_NoEngagement = 0,
  kReadCookies_NoEngagement = 1,
  kWriteCookies_NoEngagement = 2,
  kReadWriteCookies_NoEngagement = 3,
  kNoCookies_HasEngagement = 4,
  kReadCookies_HasEngagement = 5,
  kWriteCookies_HasEngagement = 6,
  kReadWriteCookies_HasEngagement = 7,
  kUnknownCookies_NoEngagement = 8,
  kUnknownCookies_HasEngagement = 9,
  kMaxValue = kUnknownCookies_HasEngagement,
};

// BtmErrorCode is used in UMA enum histograms to monitor certain errors and
// verify that they are being fixed.
//
// When adding an error to this enum, update the DIPSErrorCode enum in
// tools/metrics/histograms/enums.xml as well.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BtmErrorCode {
  kRead_None = 0,
  kRead_OpenEndedRange_NullStart = 1,
  kRead_OpenEndedRange_NullEnd = 2,
  kRead_BounceTimesIsntSupersetOfStatefulBounces = 3,
  kRead_EmptySite_InDb = 4,
  kRead_EmptySite_NotInDb = 5,
  kWrite_None = 6,
  kWrite_EmptySite = 7,
  kMaxValue = kWrite_EmptySite,
};

// BtmDeletionAction is used in UMA enum histograms to record the actual
// deletion action taken on DIPS-eligible (incidental) site.
//
// When adding an action to this enum, update the DIPSDeletionAction enum in
// tools/metrics/histograms/enums.xml as well.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BtmDeletionAction {
  kDisallowed = 0,
  kExceptedAs1p = 1,  // No longer used - merged into 'kExcepted' below.
  kExceptedAs3p = 2,  // No longer used - merged into 'kExcepted' below.
  kEnforced = 3,
  kIgnored = 4,
  kExcepted = 5,
  kMaxValue = kExcepted,
};

enum class BtmDatabaseTable {
  kBounces = 1,
  kPopups = 2,
  kMaxValue = kPopups,
};

}  // namespace content

#endif  // CONTENT_BROWSER_BTM_BTM_UTILS_H_
