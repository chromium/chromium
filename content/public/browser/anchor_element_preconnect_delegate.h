// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANCHOR_ELEMENT_PRECONNECT_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_ANCHOR_ELEMENT_PRECONNECT_DELEGATE_H_

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// Interface for preloading preconnets that are selected based on heuristic.
// TODO(isaboori): It is preferred to migrate the preconnect logic to content/
// and use NetworkContext::PreconnectSockets directly. Since, preloading
// preconnects should respect user settings regarding preloading, this migration
// also requires exposing prefetch::IsSomePreloadingEnabled to content/ as well.
class CONTENT_EXPORT AnchorElementPreconnectDelegate {
 public:
  virtual ~AnchorElementPreconnectDelegate() = default;

  // Preconnects to the given URL `target`.
  virtual void MaybePreconnect(const GURL& target) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANCHOR_ELEMENT_PRECONNECT_DELEGATE_H_
