// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVABLE_STATE_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVABLE_STATE_H_

#include <ostream>

#include "content/common/content_export.h"

namespace content {

// TODO(crbug.com/372186548): Revisit the shape of `PrefetchServableState`.
//
// See also https://crrev.com/c/5831122
enum class PrefetchServableState {
  // `PrefetchService` is checking eligibility of the prefetch or waiting load
  // start after eligibility check.
  //
  // Prefetch matching process should block until eligibility is got (and load
  // start) not to fall back normal navigation without waiting prefetch ahead of
  // prerender and send a duplicated fetch request.
  //
  // This state occurs only if `kPrerender2FallbackPrefetchSpecRules` is
  // enabled. Otherwise, `kNotServable` is returned for this period.
  kShouldBlockUntilEligibilityGot,

  // The load is started but non redirect header is not received yet.
  //
  // Prefetch matching process should block until the head of this is received
  // on a navigation to a matching URL, as a server can send a response header
  // including NoVarySearch header that contradicts NoVarySearch hint.
  kShouldBlockUntilHeadReceived,

  // This received non redirect header and is not expired.
  //
  // Note that it needs more checks to serve, e.g. cookie check. See also e.g.
  // `PrefetchMatchResolver::OnDeterminedHead()`.
  kServable,

  // Not other states.
  kNotServable,
};

CONTENT_EXPORT std::ostream& operator<<(std::ostream& ostream,
                                        PrefetchServableState servable_state);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVABLE_STATE_H_
