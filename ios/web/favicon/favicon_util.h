// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_FAVICON_FAVICON_UTIL_H_
#define IOS_WEB_FAVICON_FAVICON_UTIL_H_

#include "ios/web/public/favicon/favicon_url.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace web {

// Extracts the favicon url out of the |favicon_url_message| and put them into
// the |out_parameter|. The |page_origin| is used to get the default favicon.ico
// at the root of the page if there is none in the message. The message is
// structured as containing a list of favicons containing the href, rel and
// sizes attributes of the favicons. Returns whether the extraction was
// completely successful or not.
bool ExtractFaviconURL(const base::DictionaryValue* favicon_url_message,
                       const GURL& page_origin,
                       std::vector<web::FaviconURL>* out_parameter);

}  // namespace web
#endif  // IOS_WEB_FAVICON_FAVICON_UTIL_H_
