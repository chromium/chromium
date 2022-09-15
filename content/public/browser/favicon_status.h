// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FAVICON_STATUS_H_
#define CONTENT_PUBLIC_BROWSER_FAVICON_STATUS_H_

#include "content/common/content_export.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace content {

// Collects the favicon related information for a NavigationEntry.
struct CONTENT_EXPORT FaviconStatus {
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

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FAVICON_STATUS_H_
