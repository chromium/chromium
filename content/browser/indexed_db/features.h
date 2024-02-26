// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_FEATURES_H_
#define CONTENT_BROWSER_INDEXED_DB_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content::features {

CONTENT_EXPORT BASE_DECLARE_FEATURE(kIndexedDBShardBackingStores);

}  // namespace content::features

#endif  // CONTENT_BROWSER_INDEXED_DB_FEATURES_H_
