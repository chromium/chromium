// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVING_HANDLE_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVING_HANDLE_H_

#include "content/browser/preloading/prefetch/prefetch_container.h"

namespace content {

// TODO(https://crbug.com/437416134): Introduce `class PrefetchServingHandle`.
using PrefetchServingHandle = PrefetchContainer::Reader;

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVING_HANDLE_H_
