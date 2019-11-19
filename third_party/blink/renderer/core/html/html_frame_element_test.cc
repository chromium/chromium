// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_frame_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class HTMLFrameElementTest : public testing::Test {};

// Test that the correct container policy is constructed on a frame element.
// Frame elements do not have any container-policy related attributes, but the
// fullscreen feature should be unconditionally disabled.
TEST_F(HTMLFrameElementTest, DefaultContainerPolicy) {
  const KURL document_url("http://example.com");
  DocumentInit init =
      DocumentInit::Create()
          .WithInitiatorOrigin(SecurityOrigin::Create(document_url))
          .WithURL(document_url);
  auto* document = MakeGarbageCollected<Document>(init);

  auto* frame_element = MakeGarbageCollected<HTMLFrameElement>(*document);

  frame_element->setAttribute(html_names::kSrcAttr, "http://example.net/");
  frame_element->UpdateContainerPolicyForTests();

  const ParsedFeaturePolicy& container_policy =
      frame_element->GetFramePolicy().container_policy;
  EXPECT_EQ(1UL, container_policy.size());
  // Fullscreen should be disabled in this frame
  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen,
            container_policy[0].feature);
  EXPECT_EQ(0UL, container_policy[0].values.size());
  EXPECT_GE(PolicyValue(false), container_policy[0].fallback_value);
}

}  // namespace blink
