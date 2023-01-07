// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_VIEW_CONTENT_TYPE_UTIL_H_
#define IOS_WEB_WEB_VIEW_CONTENT_TYPE_UTIL_H_

#include <string>

namespace web {

// Returns true if `mime_type` is one of:
//   1. text/html;
//   2. application/xhtml+xml;
//   3. application/xml.
bool IsContentTypeHtml(const std::string& mime_type);
// Returns true if `mime_type` begins with "image".
bool IsContentTypeImage(const std::string& mime_type);

}  // namespace web

#endif  // IOS_WEB_WEB_VIEW_CONTENT_TYPE_UTIL_H_
