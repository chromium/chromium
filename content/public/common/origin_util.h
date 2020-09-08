// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_ORIGIN_UTIL_H_
#define CONTENT_PUBLIC_COMMON_ORIGIN_UTIL_H_

#include "content/common/content_export.h"
#include "url/origin.h"

class GURL;

namespace content {

// Returns true if the origin can register a service worker.  Scheme must be
// http (localhost only), https, or a custom-set secure scheme.
bool CONTENT_EXPORT OriginCanAccessServiceWorkers(const GURL& url);

// This is based on SecurityOrigin::isPotentiallyTrustworthy and tries to mimic
// its behavior.
//
// TODO(lukasza): Remove this function and use
// network::IsOriginPotentiallyTrustworthy instead.
bool CONTENT_EXPORT IsPotentiallyTrustworthyOrigin(const url::Origin& origin);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_ORIGIN_UTIL_H_
