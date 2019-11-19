// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/favicon/favicon_url.h"

namespace web {

FaviconURL::FaviconURL() : icon_type(IconType::kInvalid) {}

FaviconURL::FaviconURL(const GURL& url,
                       IconType type,
                       const std::vector<gfx::Size>& sizes)
    : icon_url(url), icon_type(type), icon_sizes(sizes) {}

FaviconURL::FaviconURL(const FaviconURL& other) = default;

FaviconURL::~FaviconURL() {}

}  // namespace web
