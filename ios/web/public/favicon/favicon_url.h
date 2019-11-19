// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_FAVICON_FAVICON_URL_H_
#define IOS_WEB_PUBLIC_FAVICON_FAVICON_URL_H_

#include <vector>

#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace web {

// The favicon url from the render.
struct FaviconURL {
  // The icon type in a page. The definition must be same as
  // favicon_base::IconType.
  enum class IconType {
    kInvalid = 0,
    kFavicon,
    kTouchIcon,
    kTouchPrecomposedIcon,
    kWebManifestIcon,
  };

  FaviconURL();
  FaviconURL(const GURL& url,
             IconType type,
             const std::vector<gfx::Size>& sizes);
  FaviconURL(const FaviconURL& other);
  ~FaviconURL();

  // The url of the icon.
  GURL icon_url;

  // The type of the icon
  IconType icon_type;

  // Icon's bitmaps' size
  std::vector<gfx::Size> icon_sizes;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_FAVICON_FAVICON_URL_H_
