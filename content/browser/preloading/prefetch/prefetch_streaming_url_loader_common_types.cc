// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader_common_types.h"

namespace content {

PrefetchUpdateHeadersParams::PrefetchUpdateHeadersParams() = default;
PrefetchUpdateHeadersParams::~PrefetchUpdateHeadersParams() = default;
PrefetchUpdateHeadersParams::PrefetchUpdateHeadersParams(
    PrefetchUpdateHeadersParams&&) = default;
PrefetchUpdateHeadersParams& PrefetchUpdateHeadersParams::operator=(
    PrefetchUpdateHeadersParams&&) = default;

}  // namespace content
