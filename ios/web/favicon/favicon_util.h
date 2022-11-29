// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_FAVICON_FAVICON_UTIL_H_
#define IOS_WEB_FAVICON_FAVICON_UTIL_H_

#include "base/values.h"
#include "ios/web/public/favicon/favicon_url.h"

namespace web {

// Extracts the favicon url out of `favicons` and put them into
// the `out_parameter`. The `page_origin` is used to get the default favicon.ico
// at the root of the page if there is none in the message. The message is
// structured as containing a list of favicons containing the href, rel and
// sizes attributes of the favicons. Returns whether the extraction was
// completely successful or not.
bool ExtractFaviconURL(const base::Value::List& favicons,
                       const GURL& page_origin,
                       std::vector<web::FaviconURL>* out_parameter);

}  // namespace web
#endif  // IOS_WEB_FAVICON_FAVICON_UTIL_H_
