// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_FAVICON_FAVICON_STATUS_H_
#define IOS_WEB_PUBLIC_FAVICON_FAVICON_STATUS_H_

#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace web {

// Collects the favicon related information for a NavigationItem.
struct FaviconStatus {
  FaviconStatus();

  // Indicates whether we've gotten an official favicon for the page, or are
  // just using the default favicon.
  bool valid;

  // The URL of the favicon which was used to load it off the web.
  GURL url;

  // The favicon bitmap for the page. If the favicon has not been explicitly
  // set or it empty, it will return the default favicon. Note that this is
  // loaded asynchronously, so even if the favicon URL is valid we may return
  // the default favicon if we haven't gotten the data yet.
  gfx::Image image;

  // Copy and assignment is explicitly allowed for this struct.
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_FAVICON_FAVICON_STATUS_H_
