// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class PropertyRegistrationTest : public PageTestBase {
 public:
};

TEST_F(PropertyRegistrationTest, VarInInitialValueTypedDeclared) {
  css_test_helpers::DeclareProperty(GetDocument(), "--valid", "<length>", "0px",
                                    false);
  EXPECT_TRUE(PropertyRegistration::From(GetDocument().GetExecutionContext(),
                                         AtomicString("--valid")));

  css_test_helpers::DeclareProperty(GetDocument(), "--invalid", "<length>",
                                    "var(--x)", false);
  EXPECT_FALSE(PropertyRegistration::From(GetDocument().GetExecutionContext(),
                                          AtomicString("--invalid")));
}

TEST_F(PropertyRegistrationTest, VarInInitialValueUniversalDeclared) {
  css_test_helpers::DeclareProperty(GetDocument(), "--valid", "*", "0px",
                                    false);
  EXPECT_TRUE(PropertyRegistration::From(GetDocument().GetExecutionContext(),
                                         AtomicString("--valid")));

  css_test_helpers::DeclareProperty(GetDocument(), "--invalid", "*", "var(--x)",
                                    false);
  EXPECT_FALSE(PropertyRegistration::From(GetDocument().GetExecutionContext(),
                                          AtomicString("--invalid")));
}

TEST_F(PropertyRegistrationTest, VarInInitialValueTypedRegistered) {
  css_test_helpers::RegisterProperty(GetDocument(), "--valid", "<length>",
                                     "0px", false);
  EXPECT_TRUE(PropertyRegistration::From(GetDocument().GetExecutionContext(),
                                         AtomicString("--valid")));

  DummyExceptionStateForTesting exception_state;
  css_test_helpers::RegisterProperty(GetDocument(), "--invalid", "<length>",
                                     "var(--x)", false, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_FALSE(PropertyRegistration::From(GetDocument().GetExecutionContext(),
                                          AtomicString("--invalid")));
}

TEST_F(PropertyRegistrationTest, VarInInitialValueUniversalRegistered) {
  css_test_helpers::RegisterProperty(GetDocument(), "--valid", "*", "0px",
                                     false);
  EXPECT_TRUE(PropertyRegistration::From(GetDocument().GetExecutionContext(),
                                         AtomicString("--valid")));

  DummyExceptionStateForTesting exception_state;
  css_test_helpers::RegisterProperty(GetDocument(), "--invalid", "*",
                                     "var(--x)", false, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_FALSE(PropertyRegistration::From(GetDocument().GetExecutionContext(),
                                          AtomicString("--invalid")));
}

}  // namespace blink
