// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_FAVICON_FAVICON_UTIL_H_
#define IOS_WEB_FAVICON_FAVICON_UTIL_H_

#include <vector>

#include "base/values.h"
#include "ios/web/public/favicon/favicon_url.h"
#include "url/gurl.h"

namespace web {

// Extracts the favicon urls out of `favicons`. The `page_url` is used to get
// the default "favicon.ico" at the root of the page, if there is none in the
// message. It is also used to check whether the favicon url is acceptable.
//
// The `favicons` message is structured as containing a list of dictionaries
// containing the `href`, `rel` and `size` attributes of the favicon. Will
// skip any malformed favicons in the message.
std::vector<web::FaviconURL> ExtractFaviconURL(const base::ListValue& favicons,
                                               const GURL& page_url);

}  // namespace web
#endif  // IOS_WEB_FAVICON_FAVICON_UTIL_H_
