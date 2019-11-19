// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/favicon_status.h"

#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "ui/gfx/image/image_skia.h"

namespace content {

FaviconStatus::FaviconStatus() : valid(false) {
  image = gfx::Image(GetContentClient()->browser()->GetDefaultFavicon());
}

}  // namespace content
