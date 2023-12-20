// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/geometry/dom_matrix.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_matrix_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(DOMMatrixTest, Fixup) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMMatrixInit* init = DOMMatrixInit::Create();

  EXPECT_FALSE(init->hasA());
  EXPECT_FALSE(init->hasB());
  EXPECT_FALSE(init->hasC());
  EXPECT_FALSE(init->hasD());
  EXPECT_FALSE(init->hasE());
  EXPECT_FALSE(init->hasF());
  EXPECT_FALSE(init->hasM11());
  EXPECT_FALSE(init->hasM12());
  EXPECT_FALSE(init->hasM21());
  EXPECT_FALSE(init->hasM22());
  EXPECT_FALSE(init->hasM41());
  EXPECT_FALSE(init->hasM42());

  init->setA(1.0);
  init->setB(2.0);
  init->setC(3.0);
  init->setD(4.0);
  init->setE(5.0);
  init->setF(6.0);

  EXPECT_TRUE(init->hasA());
  EXPECT_TRUE(init->hasB());
  EXPECT_TRUE(init->hasC());
  EXPECT_TRUE(init->hasD());
  EXPECT_TRUE(init->hasE());
  EXPECT_TRUE(init->hasF());
  EXPECT_FALSE(init->hasM11());
  EXPECT_FALSE(init->hasM12());
  EXPECT_FALSE(init->hasM21());
  EXPECT_FALSE(init->hasM22());
  EXPECT_FALSE(init->hasM41());
  EXPECT_FALSE(init->hasM42());

  DOMMatrix::fromMatrix(init, scope.GetExceptionState());

  EXPECT_TRUE(init->hasA());
  EXPECT_TRUE(init->hasB());
  EXPECT_TRUE(init->hasC());
  EXPECT_TRUE(init->hasD());
  EXPECT_TRUE(init->hasE());
  EXPECT_TRUE(init->hasF());
  EXPECT_TRUE(init->hasM11());
  EXPECT_TRUE(init->hasM12());
  EXPECT_TRUE(init->hasM21());
  EXPECT_TRUE(init->hasM22());
  EXPECT_TRUE(init->hasM41());
  EXPECT_TRUE(init->hasM42());
  EXPECT_EQ(1.0, init->m11());
  EXPECT_EQ(2.0, init->m12());
  EXPECT_EQ(3.0, init->m21());
  EXPECT_EQ(4.0, init->m22());
  EXPECT_EQ(5.0, init->m41());
  EXPECT_EQ(6.0, init->m42());
}

TEST(DOMMatrixTest, FixupWithFallback) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMMatrixInit* init = DOMMatrixInit::Create();

  EXPECT_FALSE(init->hasA());
  EXPECT_FALSE(init->hasB());
  EXPECT_FALSE(init->hasC());
  EXPECT_FALSE(init->hasD());
  EXPECT_FALSE(init->hasE());
  EXPECT_FALSE(init->hasF());
  EXPECT_FALSE(init->hasM11());
  EXPECT_FALSE(init->hasM12());
  EXPECT_FALSE(init->hasM21());
  EXPECT_FALSE(init->hasM22());
  EXPECT_FALSE(init->hasM41());
  EXPECT_FALSE(init->hasM42());

  DOMMatrix::fromMatrix(init, scope.GetExceptionState());

  EXPECT_TRUE(init->hasM11());
  EXPECT_TRUE(init->hasM12());
  EXPECT_TRUE(init->hasM21());
  EXPECT_TRUE(init->hasM22());
  EXPECT_TRUE(init->hasM41());
  EXPECT_TRUE(init->hasM42());
  EXPECT_EQ(1.0, init->m11());
  EXPECT_EQ(0.0, init->m12());
  EXPECT_EQ(0.0, init->m21());
  EXPECT_EQ(1.0, init->m22());
  EXPECT_EQ(0.0, init->m41());
  EXPECT_EQ(0.0, init->m42());
}

TEST(DOMMatrixTest, ThrowExceptionIfTwoValuesAreDifferent) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  {
    DOMMatrixInit* init = DOMMatrixInit::Create();
    init->setA(1.0);
    init->setM11(2.0);
    DOMMatrix::fromMatrix(init, scope.GetExceptionState());
    EXPECT_TRUE(scope.GetExceptionState().HadException());
  }
  {
    DOMMatrixInit* init = DOMMatrixInit::Create();
    init->setB(1.0);
    init->setM12(2.0);
    DOMMatrix::fromMatrix(init, scope.GetExceptionState());
    EXPECT_TRUE(scope.GetExceptionState().HadException());
  }
  {
    DOMMatrixInit* init = DOMMatrixInit::Create();
    init->setC(1.0);
    init->setM21(2.0);
    DOMMatrix::fromMatrix(init, scope.GetExceptionState());
    EXPECT_TRUE(scope.GetExceptionState().HadException());
  }
  {
    DOMMatrixInit* init = DOMMatrixInit::Create();
    init->setD(1.0);
    init->setM22(2.0);
    DOMMatrix::fromMatrix(init, scope.GetExceptionState());
    EXPECT_TRUE(scope.GetExceptionState().HadException());
  }
  {
    DOMMatrixInit* init = DOMMatrixInit::Create();
    init->setE(1.0);
    init->setM41(2.0);
    DOMMatrix::fromMatrix(init, scope.GetExceptionState());
    EXPECT_TRUE(scope.GetExceptionState().HadException());
  }
  {
    DOMMatrixInit* init = DOMMatrixInit::Create();
    init->setF(1.0);
    init->setM42(2.0);
    DOMMatrix::fromMatrix(init, scope.GetExceptionState());
    EXPECT_TRUE(scope.GetExceptionState().HadException());
  }
}

}  // namespace blink
