// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_permission_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class HTMLPemissionElementTest : public testing::Test {};

TEST_F(HTMLPemissionElementTest, SetTypeAttribute) {
  ScopedPermissionElementForTest scoped_feature(true);
  const KURL document_url("http://example.com");
  ScopedNullExecutionContext execution_context;
  auto* document = MakeGarbageCollected<Document>(
      DocumentInit::Create()
          .ForTest(execution_context.GetExecutionContext())
          .WithURL(document_url));

  auto* permission_element =
      MakeGarbageCollected<HTMLPermissionElement>(*document);
  permission_element->setAttribute(html_names::kTypeAttr,
                                   AtomicString("camera"));
  permission_element->setAttribute(html_names::kTypeAttr,
                                   AtomicString("geolocation"));

  EXPECT_EQ(AtomicString("camera"), permission_element->GetType());
}

}  // namespace blink
