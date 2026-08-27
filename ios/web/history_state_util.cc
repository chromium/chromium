// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/history_state_util.h"

#include "base/check.h"
#include "base/feature.h"
#include "base/feature_list.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web {
namespace history_state_util {

// Default enabled feature flag to control the non-standard URL fix. This is
// intended for use as a kill switch.
// TODO(crbug.com/553000452): Remove this flag once it's confirmed that the
// non-standard URL fix is working as intended.
BASE_FEATURE(kHistoryStateChangeNonStandardURLFix,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsHistoryStateChangeValid(const GURL& current_url, const GURL& to_url) {
  // These two checks are very important to the security of the page. We cannot
  // allow the page to change the state to an invalid URL.
  CHECK(current_url.is_valid());
  CHECK(to_url.is_valid());

  if (base::FeatureList::IsEnabled(kHistoryStateChangeNonStandardURLFix)) {
    return url::IsSameOriginWith(current_url, to_url);
  }

  return to_url.DeprecatedGetOriginAsURL() ==
         current_url.DeprecatedGetOriginAsURL();
}

GURL GetHistoryStateChangeUrl(const GURL& current_url,
                              const GURL& base_url,
                              std::string_view destination) {
  if (!current_url.is_valid() || !base_url.is_valid()) {
    return GURL();
  }
  GURL to_url = base_url.Resolve(destination);

  if (!to_url.is_valid() || !IsHistoryStateChangeValid(current_url, to_url)) {
    return GURL();
  }

  return to_url;
}

}  // namespace history_state_util
}  // namespace web
