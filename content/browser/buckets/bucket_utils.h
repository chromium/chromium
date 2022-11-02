// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BUCKETS_BUCKET_UTILS_H_
#define CONTENT_BROWSER_BUCKETS_BUCKET_UTILS_H_

#include "content/common/content_export.h"

#include <string>

namespace content {
CONTENT_EXPORT bool IsValidBucketName(const std::string& name);
}

#endif  // CONTENT_BROWSER_BUCKETS_BUCKET_UTILS_H_
