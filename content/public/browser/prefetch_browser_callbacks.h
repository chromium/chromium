// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PREFETCH_BROWSER_CALLBACKS_H_
#define CONTENT_PUBLIC_BROWSER_PREFETCH_BROWSER_CALLBACKS_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"

namespace content {

enum class PrefetchStartResultCode {
  // Prefetch was started successfully (i.e. the URL loader was created &
  // started).
  kSuccess = 0,

  // Prefetch failed to start.
  kFailed = 1,
};

// Used to report when a prefetch request has either started (i.e. the URL
// loader has been created & started) or failed to start. This interface is
// experimental and is subject to change, therefore it is not recommended to use
// this outside of WebView app initiated prefetching.
using PrefetchStartCallback =
    base::OnceCallback<void(const PrefetchStartResultCode)>;

}  // namespace content
#endif  // CONTENT_PUBLIC_BROWSER_PREFETCH_BROWSER_CALLBACKS_H_
