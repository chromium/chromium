// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_view/content_type_util.h"

namespace web {

bool IsContentTypeHtml(const std::string& mime_type) {
  return mime_type == "text/html" || mime_type == "application/xhtml+xml" ||
         mime_type == "application/xml";
}

bool IsContentTypeImage(const std::string& mime_type) {
  const std::string image = "image";
  return mime_type.compare(0, image.size(), image) == 0;
}

}  // namespace web
