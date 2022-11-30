// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_URL_UTIL_H_
#define IOS_WEB_COMMON_URL_UTIL_H_

#include "url/gurl.h"

namespace web {

// Removes the # (if any) and everything following it from a URL.
GURL GURLByRemovingRefFromGURL(const GURL& full_url);

}  // namespace web

#endif  // IOS_WEB_COMMON_URL_UTIL_H_
