// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/document_policy.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(CSSInterpolationTypesMapTest, RegisteredCustomProperty) {
  test::TaskEnvironment task_environment;
  auto* execution_context = MakeGarbageCollected<NullExecutionContext>();
  execution_context->SetUpSecurityContextForTesting();
  execution_context->GetSecurityContext().SetDocumentPolicy(
      DocumentPolicy::CreateWithHeaderPolicy({}));

  DocumentInit init = DocumentInit::Create()
                          .WithExecutionContext(execution_context)
                          .WithAgent(*execution_context->GetAgent());
  auto* document1 = MakeGarbageCollected<Document>(init);
  auto* document2 = MakeGarbageCollected<Document>(init);

  AtomicString property_name("--x");
  PropertyRegistration* registration =
      css_test_helpers::CreateLengthRegistration(property_name, 0);
  PropertyRegistry* registry = MakeGarbageCollected<PropertyRegistry>();
  registry->RegisterProperty(property_name, *registration);

  CSSInterpolationTypesMap map1(nullptr, *document1);
  CSSInterpolationTypesMap map2(registry, *document2);

  PropertyHandle handle(property_name);
  auto& types1 = map1.Get(handle);
  auto& types2 = map2.Get(handle);
  EXPECT_NE(&types1, &types2);
  EXPECT_EQ(types1.size(), 1u);

  auto& types1_1 = map1.Get(handle);
  EXPECT_EQ(&types1, &types1_1);

  execution_context->NotifyContextDestroyed();
}

}  // namespace blink
