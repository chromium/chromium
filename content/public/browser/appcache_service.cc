// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/appcache_service.h"

#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

namespace content {

AppCacheInfoCollection::AppCacheInfoCollection() = default;
AppCacheInfoCollection::~AppCacheInfoCollection() = default;

AppCacheService::~AppCacheService() = default;

}  // namespace content
