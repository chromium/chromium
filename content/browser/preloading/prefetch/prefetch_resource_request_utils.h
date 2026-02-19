// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_RESOURCE_REQUEST_UTILS_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_RESOURCE_REQUEST_UTILS_H_

#include "net/traffic_annotation/network_traffic_annotation.h"

namespace content {

// Avoid using `inline constexpr` here in order to place the definition to
// `.cc` file to get `tools/traffic_annotation/scripts/auditor/auditor.py` to
// work (See crbug.com/484967082 for more details).
extern const net::NetworkTrafficAnnotationTag
    kNavigationalPrefetchTrafficAnnotation;

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_RESOURCE_REQUEST_UTILS_H_
