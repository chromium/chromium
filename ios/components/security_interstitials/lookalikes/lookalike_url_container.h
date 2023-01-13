// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_LOOKALIKES_LOOKALIKE_URL_CONTAINER_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_LOOKALIKES_LOOKALIKE_URL_CONTAINER_H_

#include <set>
#include <string>
#include <vector>

#include "components/lookalikes/core/lookalike_url_util.h"
#include "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state_user_data.h"
#include "url/gurl.h"

namespace web {
class WebState;
}

// Helper object that holds URL parameters for handling interstitial reloads and
// information about the navigation used to populate the interstitial page.
class LookalikeUrlContainer
    : public web::WebStateUserData<LookalikeUrlContainer> {
 public:
  // LookalikeUrlContainer is move-only.
  LookalikeUrlContainer(LookalikeUrlContainer&& other);
  LookalikeUrlContainer& operator=(LookalikeUrlContainer&& other);
  ~LookalikeUrlContainer() override;

  struct InterstitialParams {
    // Only a limited amount of information is stored here. This might not be
    // sufficient to construct the original navigation in some edge cases (e.g.
    // POST'd to the lookalike URL, which then redirected). However, the
    // original navigation will be blocked with an interstitial, so this is an
    // acceptable compromise.
    GURL url;
    std::vector<GURL> redirect_chain;
    web::Referrer referrer;

    InterstitialParams();
    InterstitialParams(const InterstitialParams& other);
    ~InterstitialParams();
  };

  // Structure that contains information for the lookalike URL blocking page UI.
  struct LookalikeUrlInfo {
    const GURL safe_url;
    const GURL request_url;
    lookalikes::LookalikeUrlMatchType match_type;

    LookalikeUrlInfo(const GURL& safe_url,
                     const GURL& request_url,
                     lookalikes::LookalikeUrlMatchType match_type);
    LookalikeUrlInfo(const LookalikeUrlInfo& other);
    ~LookalikeUrlInfo();
  };

  // Stores parameters associated with a lookalike blocking page. Must be called
  // when a lookalike blocking page is shown.
  void RecordLookalikeBlockingPageParams(
      const GURL& url,
      const web::Referrer& referrer,
      const std::vector<GURL>& redirect_chain);

  // Stores URL info associated with a lookalike blocking page.
  void SetLookalikeUrlInfo(const GURL& safe_url,
                           const GURL& request_url,
                           lookalikes::LookalikeUrlMatchType match_type);

  // Returns currently stored parameters associated with a lookalike blocking
  // page, transferring ownership to the caller.
  std::unique_ptr<InterstitialParams> ReleaseInterstitialParams();

  // Returns currently stored URL info associated with a lookalike blocking
  // page, transferring ownership to the caller.
  std::unique_ptr<LookalikeUrlInfo> ReleaseLookalikeUrlInfo();

 private:
  explicit LookalikeUrlContainer(web::WebState* web_state);
  friend class web::WebStateUserData<LookalikeUrlContainer>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // Parameters associated with the currently displayed blocking page. These are
  // cleared immediately on next navigation.
  std::unique_ptr<InterstitialParams> interstitial_params_;

  // Lookalike URL info associated with the currently displayed blocking page.
  // These are cleared immediately on next navigation.
  std::unique_ptr<LookalikeUrlInfo> lookalike_info_;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_LOOKALIKES_LOOKALIKE_URL_CONTAINER_H_
