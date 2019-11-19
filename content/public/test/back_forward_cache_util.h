// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BACK_FORWARD_CACHE_UTIL_H_
#define CONTENT_PUBLIC_TEST_BACK_FORWARD_CACHE_UTIL_H_

#include "base/strings/string_piece.h"

namespace content {
class BackForwardCacheImpl;
class RenderFrameHost;

// Returns true if |render_frame_host| is currently stored in the
// BackForwardCache.
bool IsInBackForwardCache(RenderFrameHost* render_frame_host)
    WARN_UNUSED_RESULT;

// This is a helper class to check in the tests that back-forward cache
// was disabled for a particular reason.
//
// This class should be created in the beginning of the test and will
// know about all BackForwardCache::DisableForRenderFrameHost which
// happened during its lifetime.
//
// Typical usage pattern:
//
// BackForwardCacheDisabledTester helper;
// NavigateToURL(page_with_feature);
// NavigateToURL(away);
// EXPECT_TRUE/FALSE(helper.IsDisabledForFrameWithReason());

class BackForwardCacheDisabledTester {
 public:
  BackForwardCacheDisabledTester();
  ~BackForwardCacheDisabledTester();

  bool IsDisabledForFrameWithReason(int process_id,
                                    int frame_routing_id,
                                    base::StringPiece reason);

 private:
  // Impl has to inherit from BackForwardCacheImpl, which is
  // a content/-internal concept, so we can include it only from
  // .cc file.
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BACK_FORWARD_CACHE_UTIL_H_
