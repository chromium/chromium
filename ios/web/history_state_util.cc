// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/history_state_util.h"

#include "base/check.h"
#include "url/gurl.h"

namespace web {
namespace history_state_util {

bool IsHistoryStateChangeValid(const GURL& current_url, const GURL& to_url) {
  // These two checks are very important to the security of the page. We cannot
  // allow the page to change the state to an invalid URL.
  CHECK(current_url.is_valid());
  CHECK(to_url.is_valid());

  return to_url.DeprecatedGetOriginAsURL() ==
         current_url.DeprecatedGetOriginAsURL();
}

GURL GetHistoryStateChangeUrl(const GURL& current_url,
                              const GURL& base_url,
                              const std::string& destination) {
  if (!base_url.is_valid())
    return GURL();
  GURL to_url = base_url.Resolve(destination);

  if (!to_url.is_valid() || !IsHistoryStateChangeValid(current_url, to_url))
    return GURL();

  return to_url;
}

}  // namespace history_state_util
}  // namespace web
