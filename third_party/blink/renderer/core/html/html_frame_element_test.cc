// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_frame_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class HTMLFrameElementTest : public testing::Test {
  test::TaskEnvironment task_environment_;
};

// Test that the correct container policy is constructed on a frame element.
// Frame elements do not have any container-policy related attributes, but the
// fullscreen feature should be unconditionally disabled.
TEST_F(HTMLFrameElementTest, DefaultContainerPolicy) {
  const KURL document_url("http://example.com");
  ScopedNullExecutionContext execution_context;
  auto* document = MakeGarbageCollected<Document>(
      DocumentInit::Create()
          .ForTest(execution_context.GetExecutionContext())
          .WithURL(document_url));

  auto* frame_element = MakeGarbageCollected<HTMLFrameElement>(*document);

  frame_element->setAttribute(html_names::kSrcAttr,
                              AtomicString("http://example.net/"));
  frame_element->UpdateContainerPolicyForTests();

  const ParsedPermissionsPolicy& container_policy =
      frame_element->GetFramePolicy().container_policy;
  EXPECT_EQ(2UL, container_policy.size());
  // Fullscreen should be disabled in this frame
  EXPECT_EQ(mojom::blink::PermissionsPolicyFeature::kFullscreen,
            container_policy[0].feature);
  EXPECT_TRUE(container_policy[0].allowed_origins.empty());
  EXPECT_GE(false, container_policy[0].matches_all_origins);
}

}  // namespace blink
