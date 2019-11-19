// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PAGE_ZOOM_H_
#define CONTENT_PUBLIC_COMMON_PAGE_ZOOM_H_

namespace content {

// This enum is the parameter to various text/page zoom commands so we know
// what the specific zoom command is.
enum PageZoom {
  PAGE_ZOOM_OUT   = -1,
  PAGE_ZOOM_RESET = 0,
  PAGE_ZOOM_IN    = 1,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PAGE_ZOOM_H_
