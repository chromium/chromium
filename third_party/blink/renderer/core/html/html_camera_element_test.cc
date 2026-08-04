// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_camera_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/strings/grit/permission_element_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

using mojom::blink::PermissionName;

class HTMLCameraElementTest : public PageTestBase {
 public:
  HTMLCameraElementTest() = default;
};

TEST_F(HTMLCameraElementTest, DefaultConstraintsContainCameraOnly) {
  ScopedCameraAndMicrophoneElementsForTest scoped_feature(true);
  auto* element = MakeGarbageCollected<HTMLCameraElement>(GetDocument());
  GetDocument().body()->AppendChild(element);
  element->ApplyDefaultConstraints();

  const auto& descriptors = element->GetPermissionDescriptors();
  ASSERT_EQ(descriptors.size(), 1u);
  EXPECT_EQ(descriptors[0]->name, PermissionName::VIDEO_CAPTURE);
}

TEST_F(HTMLCameraElementTest, InheritsFromHTMLMediaTrackElementBase) {
  ScopedCameraAndMicrophoneElementsForTest scoped_feature(true);
  auto* element = MakeGarbageCollected<HTMLCameraElement>(GetDocument());
  EXPECT_TRUE(element->IsHTMLMediaTrackElementBase());
}

}  // namespace blink
