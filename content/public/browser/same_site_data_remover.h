// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SAME_SITE_DATA_REMOVER_H_
#define CONTENT_PUBLIC_BROWSER_SAME_SITE_DATA_REMOVER_H_

#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"

namespace content {
class BrowserContext;

// Clears cookies available in third-party contexts where SameSite=None.
// Also removes storage for domains with a SameSite=None cookie.
CONTENT_EXPORT void ClearSameSiteNoneData(base::OnceClosure closure,
                                          BrowserContext* context);

// Clears cookies available in third-party contexts where SameSite=None.
// Also removes storage for origins that match the clear_storage_origin_matcher
// predicate.
CONTENT_EXPORT void ClearSameSiteNoneCookiesAndStorageForOrigins(
    base::OnceClosure closure,
    BrowserContext* context,
    StoragePartition::OriginMatcherFunction clear_storage_origin_matcher);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SAME_SITE_DATA_REMOVER_H_
