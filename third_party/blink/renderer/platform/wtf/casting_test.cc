// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/casting.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class Base {
 public:
  virtual ~Base() = default;

  virtual bool IsDerived() const { return false; }
};

class Intermediate : public Base {};

class Derived : public Intermediate {
 public:
  bool IsDerived() const override { return true; }
};

}  // namespace

template <>
struct DowncastTraits<Derived> {
  static bool AllowFrom(const Base& base) { return base.IsDerived(); }
};

TEST(CastingTest, Basic) {
  Derived d;

  Base* b = &d;
  Intermediate* i = &d;

  EXPECT_EQ(&d, To<Derived>(b));
  EXPECT_EQ(&d, To<Derived>(i));

  Intermediate i2;
  EXPECT_FALSE(IsA<Derived>(i2));
}

}  // namespace blink
