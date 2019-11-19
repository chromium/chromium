// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_RESOURCE_TYPE_H_
#define CONTENT_PUBLIC_COMMON_RESOURCE_TYPE_H_

#include "content/common/content_export.h"

namespace content {

// Used in histograms; explicitly assign each type and do not re-use old values.
enum class ResourceType {
  kMainFrame = 0,        // top level page
  kSubFrame = 1,         // frame or iframe
  kStylesheet = 2,       // a CSS stylesheet
  kScript = 3,           // an external script
  kImage = 4,            // an image (jpg/gif/png/etc)
  kFontResource = 5,     // a font
  kSubResource = 6,      // an "other" subresource.
  kObject = 7,           // an object (or embed) tag for a plugin.
  kMedia = 8,            // a media resource.
  kWorker = 9,           // the main resource of a dedicated worker.
  kSharedWorker = 10,    // the main resource of a shared worker.
  kPrefetch = 11,        // an explicitly requested prefetch
  kFavicon = 12,         // a favicon
  kXhr = 13,             // a XMLHttpRequest
  kPing = 14,            // a ping request for <a ping>/sendBeacon.
  kServiceWorker = 15,   // the main resource of a service worker.
  kCspReport = 16,       // a report of Content Security Policy violations.
  kPluginResource = 17,  // a resource that a plugin requested.
  // kNavigationPreload = 18,  // Deprecated.
  // a main-frame service worker navigation preload request.
  kNavigationPreloadMainFrame = 19,
  // a sub-frame service worker navigation preload request.
  kNavigationPreloadSubFrame = 20,
  kMaxValue = kNavigationPreloadSubFrame,
};

CONTENT_EXPORT bool IsResourceTypeFrame(ResourceType type);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RESOURCE_TYPE_H_
