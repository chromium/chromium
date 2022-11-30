// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_FAVICON_FAVICON_STATUS_H_
#define IOS_WEB_PUBLIC_FAVICON_FAVICON_STATUS_H_

#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace web {

// Collects the favicon related information for a NavigationItem.
struct FaviconStatus {
  // Indicates whether we've gotten an official favicon for the page.
  bool valid = false;

  // The URL of the favicon which was used to load it off the web.
  GURL url;

  // The favicon bitmap for the page. It is fetched asynchronously after the
  // favicon URL is set, so it is possible for `image` to be empty even when
  // `valid` is set to true.
  gfx::Image image;

  // Copy and assignment is explicitly allowed for this struct.
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_FAVICON_FAVICON_STATUS_H_
