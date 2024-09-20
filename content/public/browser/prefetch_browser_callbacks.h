// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PREFETCH_BROWSER_CALLBACKS_H_
#define CONTENT_PUBLIC_BROWSER_PREFETCH_BROWSER_CALLBACKS_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"

namespace content {

enum class PrefetchCallbackType {
  // Called when the initial prefetch request is started (i.e. the URL loader is
  // created & started),
  // just after `PrefetchContainer` has transitioned to `LoadState::kStarted`.
  // This is called at most once per `PrefetchContainer` and is not called when
  // prefetch request redirects are started.
  kStarted
};

// Used to resolve callbacks related to the various non-failure states of a
// prefetch request.
// This interface is experimental and is subject to change, therefore
// it is not recommended to use this outside of WebView app initiated
// prefetching.
using PrefetchBrowserCallback =
    base::RepeatingCallback<void(const PrefetchCallbackType type)>;

}  // namespace content
#endif  // CONTENT_PUBLIC_BROWSER_PREFETCH_BROWSER_CALLBACKS_H_
