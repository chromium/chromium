// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FENCED_FRAME_TEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_FENCED_FRAME_TEST_UTIL_H_

#include "base/compiler_specific.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"

class GURL;

namespace content {

class RenderFrameHost;

namespace test {

// Browser tests can use this class to more conveniently leverage fenced frames.
// Note that this applies to both the MPArch and ShadowDOM version of fenced
// frames.
class FencedFrameTestHelper {
 public:
  explicit FencedFrameTestHelper();
  ~FencedFrameTestHelper();
  FencedFrameTestHelper(const FencedFrameTestHelper&) = delete;
  FencedFrameTestHelper& operator=(const FencedFrameTestHelper&) = delete;

  // This method creates a new fenced frame in `mode` rooted at
  // `fenced_frame_parent` that is navigated to `url`. This method waits for the
  // navigation to `url` to commit, and returns the `RenderFrameHost` that
  // committed the navigation if it succeeded. Otherwise, it returns `nullptr`.
  // See `NavigationFrameInFencedFrameTree()` documentation for the
  // `expected_error_code` parameter.
  RenderFrameHost* CreateFencedFrame(
      RenderFrameHost* fenced_frame_parent,
      const GURL& url,
      net::Error expected_error_code = net::OK,
      blink::mojom::FencedFrameMode mode =
          blink::mojom::FencedFrameMode::kDefault);

  // This method is similar to `FencedFrameTestHelper::CreateFencedFrame` but
  // doesn't wait until the fenced frame completes loading.
  void CreateFencedFrameAsync(RenderFrameHost* fenced_frame_parent_rfh,
                              const GURL& url);

  // This method provides a way to navigate frames within a fenced frame's tree,
  // and synchronously wait for the load to finish. This method returns the
  // `RenderFrameHost` that the navigation committed to if it was successful
  // (which may be different from the one that navigation started in), and
  // `nullptr` otherwise. It takes an `expected_error_code` in case the
  // navigation to `url` fails, which can be detected on a per-error-code basis.
  // TODO(crbug.com/1294189): Directly use TestFrameNavigationObserver instead
  // of relying on this method.
  RenderFrameHost* NavigateFrameInFencedFrameTree(
      RenderFrameHost* rfh,
      const GURL& url,
      net::Error expected_error_code = net::OK);

  // Returns the last created fenced frame. This can be used by embedders who
  // must create fenced frames from script but need to get the fence frame's
  // inner root RenderFrameHost.
  // This method will return nullptr if no fenced frames were created.
  static RenderFrameHost* GetMostRecentlyAddedFencedFrame(RenderFrameHost* rfh);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This helper method creates a fenced frame urn to url mapping and returns the
// urn in GURL format. It applies to both MPArch and ShadowDOM
// architeectures of fenced frames
GURL CreateFencedFrameURLMapping(RenderFrameHost* rfh, const GURL& url);

}  // namespace test

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FENCED_FRAME_TEST_UTIL_H_
