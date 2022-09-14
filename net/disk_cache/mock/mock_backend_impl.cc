// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/mock/mock_backend_impl.h"

namespace disk_cache {

BackendMock::BackendMock(net::CacheType cache_type) : Backend(cache_type) {}
BackendMock::~BackendMock() = default;

}  // namespace disk_cache
