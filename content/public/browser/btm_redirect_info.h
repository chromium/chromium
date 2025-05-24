// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BTM_REDIRECT_INFO_H_
#define CONTENT_PUBLIC_BROWSER_BTM_REDIRECT_INFO_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace content {

enum class BtmCookieMode { kBlock3PC, kOffTheRecord_Block3PC };

enum class BtmRedirectType { kClient, kServer };

// BtmDataAccessType:
// NOTE: We use this type as a bitfield and emit it in metrics as the
// CookieAccessType enum. Don't change the values or add additional members.
enum class BtmDataAccessType {
  kUnknown = -1,
  kNone = 0,
  kRead = 1,
  kWrite = 2,
  kReadWrite = 3
};

struct UrlAndSourceId {
  GURL url;
  ukm::SourceId source_id;
};

// Properties of a redirect chain common to all the URLs within the chain.
struct CONTENT_EXPORT BtmRedirectChainInfo {
 public:
  BtmRedirectChainInfo(const UrlAndSourceId& initial_url,
                       const UrlAndSourceId& final_url,
                       size_t length,
                       bool is_partial_chain,
                       bool are_3pcs_generally_enabled);
  BtmRedirectChainInfo(const BtmRedirectChainInfo&);
  ~BtmRedirectChainInfo();

  // A randomly-generated ID to associate redirects within the same chain for
  // metrics reporting.
  const int32_t chain_id;

  const UrlAndSourceId initial_url;
  // The eTLD+1 of initial_url, cached.
  const std::string initial_site;

  const UrlAndSourceId final_url;
  // The eTLD+1 of final_url, cached.
  const std::string final_site;

  // initial_site == final_site, cached.
  const bool initial_and_final_sites_same;
  const size_t length;
  // True if the chain is missing the end URL. This occurs when redirects are
  // trimmed from the front of the in-progress redirect chain.
  const bool is_partial_chain;
  const bool are_3pcs_generally_enabled;

  // These properties aren't known at the time of creation, and are filled in
  // later:
  std::optional<BtmCookieMode> cookie_mode;
};

// Properties of one URL within a redirect chain.
struct CONTENT_EXPORT BtmRedirectInfo {
 public:
  static std::unique_ptr<BtmRedirectInfo> CreateForServer(
      const UrlAndSourceId& url,
      BtmDataAccessType access_type,
      base::Time time,
      bool was_response_cached,
      int response_code,
      base::TimeDelta server_bounce_delay);

  static std::unique_ptr<BtmRedirectInfo> CreateForClient(
      const UrlAndSourceId& url,
      BtmDataAccessType access_type,
      base::Time time,
      base::TimeDelta client_bounce_delay,
      bool has_sticky_activation,
      bool web_authn_assertion_request_succeeded);

  BtmRedirectInfo(const BtmRedirectInfo&);
  ~BtmRedirectInfo();

  // These properties are required for all redirects:

  // Corresponds to the URL that triggers the redirection. In the case of
  // server redirects, the URL that received a redirect status code e.g. 301. In
  // the case of a client redirect, the URL of the page that initiated the
  // navigation e.g. called `window.location.href = "https://foo.example";`
  const UrlAndSourceId redirecting_url;
  const std::string site;  // The cached result of GetSiteForBtm(url).
  const BtmRedirectType redirect_type;
  BtmDataAccessType
      access_type;  // May be updated by late cookie notifications.
  const base::Time time;

  // These properties aren't known at the time of creation, and are filled in
  // later:
  std::optional<bool> site_had_user_activation;
  std::optional<size_t> chain_index;
  // See BtmRedirectChainInfo::chain_id.
  std::optional<int32_t> chain_id;
  std::optional<bool> has_3pc_exception;

  // The following properties are only applicable for client-side redirects:

  // For client redirects, the time between the previous page committing
  // and the redirect navigation starting. (For server redirects, zero)
  const base::TimeDelta client_bounce_delay;
  // For client redirects, whether the user ever interacted with the page during
  // this navigation.
  const bool has_sticky_activation;
  // For client redirects, whether the user ever triggered a web authn assertion
  // call.
  const bool web_authn_assertion_request_succeeded;

  // The following properties are only applicable for server-side redirects:
  const bool was_response_cached;
  const int response_code;
  const base::TimeDelta server_bounce_delay;

 private:
  BtmRedirectInfo(const UrlAndSourceId& url,
                  BtmRedirectType redirect_type,
                  BtmDataAccessType access_type,
                  base::Time time,
                  base::TimeDelta client_bounce_delay,
                  bool has_sticky_activation,
                  bool web_authn_assertion_request_succeeded,
                  bool was_response_cached,
                  int response_code,
                  base::TimeDelta server_bounce_delay);
};

// a movable BtmRedirectInfo, essentially
using BtmRedirectInfoPtr = std::unique_ptr<BtmRedirectInfo>;

// a movable BtmRedirectChainInfo, essentially
using BtmRedirectChainInfoPtr = std::unique_ptr<BtmRedirectChainInfo>;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BTM_REDIRECT_INFO_H_
