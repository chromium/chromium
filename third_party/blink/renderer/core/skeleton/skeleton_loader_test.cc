// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/skeleton/skeleton_loader.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class SkeletonLoaderTest : public SimTest {};

TEST_F(SkeletonLoaderTest, Basic) {
  ScopedDeclarativeSkeletonsForTest enable_skeletons(true);

  // - Create a dummy url that you add to the SkeletonLoader with
  // AddSkeletonPrefetchLink().
  KURL dummy_url("https://example.com/dummy.html");

  // Start with a basic page load so we have a valid document structure.
  SimRequest main("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main.Complete("<html><body></body></html>");

  Document& document = GetDocument();
  SkeletonLoader& loader = SkeletonLoader::Ensure(document);

  loader.AddSkeletonPrefetchLink(dummy_url);
  Compositor().BeginFrame();

  // Check that the Document does not initially have a ::skeleton pseudo-element
  Element* root = document.documentElement();
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->GetPseudoElement(kPseudoIdSkeleton), nullptr);

  loader.NavigateTo(dummy_url);
  Compositor().BeginFrame();

  // Check that the Document now has a ::skeleton pseudo-element
  EXPECT_NE(root->GetPseudoElement(kPseudoIdSkeleton), nullptr);

  // Call CancelNavigation() to remove the skeleton
  loader.CancelNavigation();
  Compositor().BeginFrame();

  // The Document should no longer have a ::skeleton pseudo-element
  EXPECT_EQ(root->GetPseudoElement(kPseudoIdSkeleton), nullptr);
}

}  // namespace blink
