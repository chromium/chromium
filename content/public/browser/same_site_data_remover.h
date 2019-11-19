// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SAME_SITE_DATA_REMOVER_H_
#define CONTENT_PUBLIC_BROWSER_SAME_SITE_DATA_REMOVER_H_

#include "base/callback.h"
#include "content/common/content_export.h"

namespace content {
class BrowserContext;

// Clears cookies available in third-party contexts where SameSite=None.
// Also removes associated storage if |clear_storage| is set to true.
CONTENT_EXPORT void ClearSameSiteNoneData(base::OnceClosure closure,
                                          BrowserContext* context,
                                          bool clear_storage);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SAME_SITE_DATA_REMOVER_H_
