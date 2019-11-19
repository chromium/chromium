// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACK_FORWARD_CACHE_H_
#define CONTENT_PUBLIC_BROWSER_BACK_FORWARD_CACHE_H_

#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

class RenderFrameHost;

// Public API for the BackForwardCache.
//
// After the user navigates away from a document, the old one might go into the
// frozen state and will be kept in the cache. It can potentially be reused
// at a later time if the user navigates back.
//
// Not all documents can or will be cached. You should not assume a document
// will be cached.
//
// WARNING: This code is still experimental and might completely go away.
// Please get in touch with bfcache-dev@chromium.org if you intend to use it.
//
// All methods of this class should be called from the UI thread.
class CONTENT_EXPORT BackForwardCache {
 public:
  // Prevents the |render_frame_host| from entering the BackForwardCache. A
  // RenderFrameHost can only enter the BackForwardCache if the main one and all
  // its children can. This action can not be undone. Any document that is
  // assigned to this same RenderFrameHost in the future will not be cached
  // either. In practice this is not a big deal as only navigations that use a
  // new frame can be cached.
  //
  // This might be needed for example by components that listen to events via a
  // WebContentsObserver and keep some sort of per frame state, as this state
  // might be lost and not be recreated when navigating back.
  //
  // If the page is already in the cache an eviction is triggered.
  //
  // |render_frame_host|: non-null.
  // |reason|: Free form string to be used in logging and metrics.
  static void DisableForRenderFrameHost(RenderFrameHost* render_frame_host,
                                        base::StringPiece reason);

  // Helper function to be used when it is not always possible to guarantee the
  // |render_frame_host| to be still alive when this is called. In this case,
  // its |id| can be used.
  static void DisableForRenderFrameHost(GlobalFrameRoutingId id,
                                        base::StringPiece reason);

  // List of reasons the BackForwardCache was disabled for a specific test. If a
  // test needs to be disabled for a reason not covered below, please add to
  // this enum.
  enum DisableForTestingReason {
    // The test has expectations that won't make sense if caching is enabled.
    //
    // One alternative to disabling BackForwardCache is to make the test's logic
    // conditional, based on whether or not BackForwardCache is enabled.
    //
    // You should also consider whether it would make sense to instead split
    // into two tests, one using a cacheable page, and one using an uncacheable
    // page.
    //
    // Once BackForwardCache is enabled everywhere, any tests still disabled for
    // this reason should change their expectations to permanently match the
    // BackForwardCache enabled behavior.
    TEST_ASSUMES_NO_CACHING,

    // Unload events never fire for documents that are put into the
    // BackForwardCache. This is by design, as there is never an appropriate
    // moment to fire unload if the document is cached.
    // In short, this is because:
    //
    // * We can't fire unload when going into the cache, because it may be
    // destructive, and put the document into an unknown/bad state. Pages can
    // also be cached and restored multiple times, and we don't want to invoke
    // unload more than once.
    //
    // * We can't fire unload when the document is evicted from the cache,
    // because at that point we don't want to run javascript for privacy and
    // security reasons.
    //
    // An alternative to disabling the BackForwardCache, is to have the test
    // load a page that is ineligible for caching (e.g. due to an unsupported
    // feature).
    TEST_USES_UNLOAD_EVENT,
  };

  // Disables the BackForwardCache so that no documents will be stored/served.
  // This allows tests to "force" not using the BackForwardCache, this can be
  // useful when:
  // * Tests rely on a new document being loaded.
  // * Tests want to test this case specifically.
  // Callers should pass an accurate |reason| to make future triaging of
  // disabled tests easier.
  //
  // Note: It's preferable to make tests BackForwardCache compatible
  // when feasible, rather than using this method. Also please consider whether
  // you actually should have 2 tests, one with the document cached
  // (BackForwardCache enabled), and one without.
  virtual void DisableForTesting(DisableForTestingReason reason) = 0;

 protected:
  BackForwardCache() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACK_FORWARD_CACHE_H_
